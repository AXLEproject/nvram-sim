#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <iostream>
#include <string.h>
#include "globals.h"
#include "cache.h"
#include "logger.h"

#define bits(x, i, l) (((x) >> (i)) & bitmask(l))

bool shutdown_started=false;
char some_temp_string[1024];
char *strbuf=some_temp_string;

static Logger nvlogger("trace.txt");

void nvlog_flush() {
  nvlogger.Flush();
}
void nvlog(char *data, int size) {
  nvlogger.Log((unsigned char *)data, size);
  nvlogger.Flush();
}

const std::string state2str (const uint8_t line_state)
{
    char strbuf[10];
    int pos = 0;
    if (line_state == 0)
    {
        strbuf[pos++]='I';
        strbuf[pos++]='\0';
    }
    else
    {
        if (line_state & LINE_SHR) strbuf[pos++] = 'S';
        if (line_state & LINE_EXC) strbuf[pos++] = 'E';
        if (line_state & LINE_MOD) strbuf[pos++] = 'M';
        if (line_state & LINE_TXR) strbuf[pos++] = 'X';
        if (line_state & LINE_TXW) strbuf[pos++] = 'D';
        strbuf[pos++] = '\0';
    }
    return std::string(strbuf);
}

void
Cache :: line_mark_in_parent(Addr addr, uint8_t line_state_req, size_t &latency)
{
    Line *line = addr2line_internal(addr);
    uint8_t *data;
    if (line) {
        this->line_get(addr, line_state_req, latency, data);
    } else {
        this->_parent->line_get_intercache(addr, line_state_req, latency, this->_index_in_parent, line);
    }
}

void
Cache :: line_get(Addr addr, uint8_t line_state_req, size_t &latency, uint8_t *&pdata)
{
    //////////////////////////////////////////////////////////////////
    //
    //  proudly serving cache requests
    //  since september 2008
    //
    //////////////////////////////////////////////////////////////////
    //
    // line_state_req(uest) values:
    // LINE_SHR - request a read
    // LINE_EXC - request an exclusive access (invalidate other readers)
    // LINE_MOD - perform a write (invalidate other readers and mark line as dirty)

    bool set_overflow = false;
    Line *overflow_line = NULL;
    // this adds a line (if it's not already in the cache) but with LINE_INV state
    Line *line = addr2line(addr, set_overflow, overflow_line);
    // if some line has been replaced, get the value and evict/invalidate the same line and its parts
    // in all child caches
    if (set_overflow) {
        NVLOG1("%s\tline_get overflow 0x%lx\n", _name.c_str(), overflow_line->addr);
        this->line_evict(overflow_line);
    }
    uint8_t __attribute__((unused)) line_state_orig = line->state;
    uint64_t __attribute__((unused)) line_sharers_orig = line->sharers;

    bool hit = true;
    size_t old_ticks = latency;

    if (line->state & LINE_MOD)
    {
        switch (line_state_req) {
            case LINE_MOD:
            case LINE_EXC:
            case LINE_SHR:
                break;
            default:
                assert(!"invalid line_state request!");
        }
    }
    else if (line->state & LINE_EXC)
    { // line is EXCLUSIVE when it is not modified and there is no other line sharer
        // TODO update the functionality to reflect this;
        // first line sharer should automatically get exclusive access;
        // this exclusive access should be reduced to shared when another core requests line copy (for read)
        switch (line_state_req) {
            case LINE_MOD:
                if (_is_writeback_cache || !_parent_cache) {
                    // mark line as dirty (for writeback)
                    line->state |= line_state_req;
                } else {
                    // write latency should include writing to the parent
                    _parent->line_get_intercache(addr, line_state_req, latency, _index_in_parent, line->parent_line);
                    // TODO should also write data to the parent cache
                    hit = false;
                    break;
                }
            case LINE_EXC:
            case LINE_SHR:
                break;
            default:
                assert(!"invalid line_state request!");
        }
    }
    else if (line->state & LINE_SHR)
    {
        switch (line_state_req) {
            case LINE_MOD:
            case LINE_EXC:
                if (_parent_cache) {
                    if (_is_writeback_cache) {
                        _parent->line_get_intercache(addr, LINE_EXC, latency, _index_in_parent, line->parent_line);
                    } else {
                        _parent->line_get_intercache(addr, line_state_req, latency, _index_in_parent, line->parent_line);
                    }
                    hit = false;
                }
                line->state |= line_state_req;
                break;
            case LINE_SHR:
                break;
            default:
                assert(!"invalid line_state request!");
        }
    }
    else if (line->state == LINE_INV)
    {
        hit = false;
        switch (line_state_req) {
            case LINE_TXW:
            case LINE_TXR:
                _parent->line_get_intercache(addr, LINE_SHR, latency, _index_in_parent, line->parent_line);
                line->state |= line->parent_line->state & ( LINE_TXR | LINE_TXW);
                line->state |= (LINE_SHR | line_state_req);
                break;
            case LINE_MOD:
            case LINE_EXC:
                if (_is_writeback_cache) {
                    _parent->line_get_intercache(addr, LINE_EXC, latency, _index_in_parent, line->parent_line);
                } else {
                    _parent->line_get_intercache(addr, line_state_req, latency, _index_in_parent, line->parent_line);
                }
                line->state |= line_state_req;
                break;
            case LINE_SHR:
                _parent->line_get_intercache(addr, LINE_SHR, latency, _index_in_parent, line->parent_line);
                line->state |= line_state_req;
                break;
            default:
                assert(!"invalid line_state request!");
        }
    }
    else
    {
        NVLOG_ERROR("invalid current line_state %x\n", line->state);
        assert(false && "invalid current line_state!");
    }


    if (line->pdata == NULL) {
        line->pdata = (uint8_t*)malloc(get_line_size());
        if (_parent_cache && line->parent_line) { // if data found in parent cache
            NVLOG1("%s\tline_get data copy from %s 0x%lx data 0x%lx -> 0x%lx\n", this->_name.c_str(), _parent_cache->_name.c_str(), line->addr, (Addr)line->parent_line->pdata, (Addr)line->pdata);
            memcpy(line->pdata, line->parent_line->pdata+(line->addr - line->parent_line->addr), get_line_size());
        } else {
            // get the data from the memory
            NVLOG1("%s\tline_get data copy from memory 0x%lx data -> 0x%lx\n", this->_name.c_str(), line->addr, (Addr)line->pdata);
	    //Fault fault = rw_array_silent(line->addr, get_line_size(), line->pdata, false/*READ*/);
	    //assert(fault == NoFault);
        }
    }
    pdata = line->pdata;

    latency += _hit_latency;
    // update statistics
    if (hit) {
        this->stats.hits_inc();
    } else {
        this->stats.misses_inc();
        if (line_state_req & LINE_MOD || line_state_req & LINE_EXC)
        { this->stats.misses_st_inc(); }
        else
        { this->stats.misses_ld_inc(); }
    }
    this->stats.ticks_inc(latency - old_ticks);
    NVLOG1("%s\tline_get 0x%lx\t state %s->%s sharers 0x%lx->0x%lx\n",  _name.c_str(),  line->addr,  state2str(line_state_orig).c_str(), state2str(line->state).c_str(), line_sharers_orig, line->sharers);
    assert(! ((line->state & (LINE_MOD | LINE_EXC)) && (line->state & LINE_TXW)) );
}

void
Cache :: line_get_intercache(Addr addr, uint8_t line_state_req, size_t &latency, unsigned child_index, Line *&parent_line)
{
    //////////////////////////////////////////////////////////////////////
    // serving line relocations inside the cache hierarchy
    //////////////////////////////////////////////////////////////////////

    bool set_overflow = false;
    Line *overflow_line = NULL;
    // this adds a line (if it's not already in the cache) but with LINE_INV state
    Line *line = addr2line(addr, set_overflow, overflow_line);
    if (set_overflow) {
        // if some line has been replaced, get the value and invalidate the same line and its parts
        // in all child caches
        NVLOG1("%s\tline_get overflow 0x%lx\n", _name.c_str(), overflow_line->addr);
        this->line_evict(overflow_line);
    }
    uint8_t __attribute__((unused)) line_state_orig = line->state;
    uint64_t __attribute__((unused)) line_sharers_orig = line->sharers;

    bool hit = true;
    size_t old_ticks = latency;

    if (line->state & (LINE_MOD | LINE_EXC))
    {
        // this directory already owns a line (exclusively)
        // just in case we'll invalidate the line in
        // all other child-caches
        switch (line_state_req) {
            case LINE_MOD:
                if (line->sharers != (1ULL<<child_index)) {
                    this->line_make_owner_in_child_caches(line, child_index);
                    setbit(line->sharers, child_index);
                }
                if (_is_writeback_cache || !_parent_cache) {
                    line->state |= line_state_req;
                } else {
                    _parent->line_get_intercache(addr, line_state_req, latency, _index_in_parent, line->parent_line);
                    hit = false;
                    break;
                }
                break;
            case LINE_EXC:
                // this processor wants to have an exclusive copy
                // (and it didn't have it until now, as we got an intercache request)
                if (line->sharers != (1ULL<<child_index)) {
                    this->line_make_owner_in_child_caches(line, child_index);
                    setbit(line->sharers, child_index);
                }
                break;
            case LINE_SHR:
                if (line->sharers != (1ULL<<child_index)) {
                    this->line_writer_to_sharer(line, latency);
                    // and do not write the data up in the memory hierarchy
                    setbit(line->sharers, child_index);
                }
                break;
            default:
                assert(!"invalid line_state request!");
        }
        //assert(line_state_req == LINE_SHR || line_state_req == LINE_EXC || line_state_req == LINE_MOD);
        line->state |= line_state_req; // we might also reduce from writer to LINE_SHR in this cache
    }
    else if (line->state & LINE_SHR)
    {
        switch (line_state_req) {
            case LINE_MOD:
            case LINE_EXC:
                this->line_make_owner_in_child_caches(line, child_index);
                setbit(line->sharers, child_index);
                if (_parent_cache) {
                    if (_is_writeback_cache) {
                        _parent->line_get_intercache(addr, LINE_EXC, latency, _index_in_parent, line->parent_line);
                    } else {
                        _parent->line_get_intercache(addr, line_state_req, latency, _index_in_parent, line->parent_line);
                    }
                    hit = false;
                }
                line->state |= line_state_req;
                break;
            case LINE_SHR:
                setbit(line->sharers, child_index);
                break;
            default:
                assert(!"invalid line_state request!");
        }
    }
    else if (line->state == LINE_INV)
    {
        hit = false;
        switch (line_state_req) {
            case LINE_MOD:
            case LINE_EXC:
                if (_is_writeback_cache) {
                    _parent->line_get_intercache(addr, LINE_EXC, latency, _index_in_parent, line->parent_line);
                } else {
                    _parent->line_get_intercache(addr, line_state_req, latency, _index_in_parent, line->parent_line);
                }
                if (line->parent_line) { line->state = line->parent_line->state; }
                line->state |= line_state_req;
                setbit(line->sharers, child_index);
                break;
            case LINE_SHR:
                _parent->line_get_intercache(addr, LINE_SHR, latency, _index_in_parent, line->parent_line);
                if (line->parent_line) { line->state = line->parent_line->state; }
                line->state |= line_state_req;
                setbit(line->sharers, child_index);
                break;
            default:
                assert(!"invalid line_state request!");
        }
    }
    else
    {
        NVLOG_ERROR("invalid current line_state %x\n", line->state);
        assert(false && "invalid current line_state!");
    }

    if (line->pdata == NULL) {
        line->pdata = (uint8_t*)malloc(get_line_size());
        if (_parent_cache && line->parent_line) { // if data found in any parent cache
            NVLOG1("%s\tline_get data copy from %s 0x%lx data 0x%lx -> 0x%lx\n", this->_name.c_str(), _parent_cache->_name.c_str(), line->addr, (Addr)line->parent_line->pdata, (Addr)line->pdata);
            memcpy(line->pdata, line->parent_line->pdata+(line->addr - line->parent_line->addr), get_line_size());
        } else {
            // get the data from the memory
            NVLOG1("%s\tline_get data copy from memory 0x%lx data 0x%lx\n", this->_name.c_str(), line->addr, (Addr)line->pdata);
//            Fault fault = rw_array_silent(line->addr, get_line_size(), line->pdata, false/*READ*/);
//            assert(fault == NoFault);
        }
    }
    parent_line = line;

    latency += _hit_latency;
    // update statistics
    if (hit) {
        this->stats.hits_inc();
    } else {
        this->stats.misses_inc();
        if (line_state_req & LINE_MOD || line_state_req & LINE_EXC)
        { this->stats.misses_st_inc(); }
        else
        { this->stats.misses_ld_inc(); }
    }
    this->stats.ticks_inc(latency - old_ticks);
    NVLOG1("%s\tline_get 0x%lx\t state %s->%s sharers 0x%lx->0x%lx\n",  _name.c_str(),  line->addr,  state2str(line_state_orig).c_str(), state2str(line->state).c_str(), line_sharers_orig, line->sharers);
    assert(! ((line->state & (LINE_MOD | LINE_EXC)) && (line->state & LINE_TXW)) );
}

bool
Cache :: line_make_owner_in_child_caches(Line *line, unsigned child_index)
{
    NVLOG1("%s\tline_make_owner_in_child_caches 0x%lx sharers %lx caller %d\n", this->_name.c_str(), line->addr, line->sharers, child_index);
    uint8_t __attribute__((unused)) line_state_orig = line->state;
    uint64_t __attribute__((unused)) line_sharers_orig = line->sharers;
    // evict the line in all but the requesting child cache
    for (size_t child_i = 0; child_i<_children.size() && line->sharers>0; child_i++) {
        Cache *child = dynamic_cast<Cache *>(_children[child_i]); assert(child!=NULL);
        // NVLOG1("%s\tchecking for 0x%lx %lx 1\n", child->_name.c_str(), line->addr, line->sharers);
        if (!bit(line->sharers, child_i)) continue; // a child is not a sharer, so continue
        // NVLOG1("%s\tchecking for 0x%lx %lx 2\n", child->_name.c_str(), line->addr, line->sharers);
        if (child_i == child_index) continue; // skip the child cache that called us
        // NVLOG1("%s\tchecking for 0x%lx %lx 3\n", child->_name.c_str(), line->addr, line->sharers);
        for (Addr line_addr_iter=line->addr; line_addr_iter<line->addr+get_line_size(); line_addr_iter += child->get_line_size())
        {
            NVLOG1("%s\tis line sharer, checking segment 0x%lx\n", child->_name.c_str(), line_addr_iter);
            Line *child_line = child->addr2line_internal(line_addr_iter);
            if (!child_line) continue;
            NVLOG1("%s\thas segment 0x%lx, evicting\n", child->_name.c_str(), line_addr_iter);
            child->line_evict(child_line);
        }
    }
    line->sharers = 0;
    setbit(line->sharers, child_index);
    NVLOG1("%s\tline 0x%lx\t state %s->%s sharers 0x%lx->0x%lx\n",  _name.c_str(),  line->addr,  state2str(line_state_orig).c_str(), state2str(line->state).c_str(), line_sharers_orig, line->sharers);
    return true;
}

void
Cache :: line_writer_to_sharer(const Addr addr, size_t &latency, bool children_only)
{
    Line *line = addr2line_internal(addr);
    if (line==NULL) return;
    this->line_writer_to_sharer(line, latency, children_only);
}

void
Cache :: line_writer_to_sharer(Line *line, size_t &latency, bool children_only)
{
    // make this cache (and child caches)
    // only sharers of the line (downgrade from a writer)
    NVLOG1("%s\tline_writer_to_sharer 0x%lx +%ld cycles\n", this->_name.c_str(), line->addr, _hit_latency);
    latency += _hit_latency;
    this->stats.writebacks_inc();
    for (size_t child_i = 0; child_i<_children.size(); child_i++) {
        if (!bit(line->sharers, child_i)) continue; // a child is not a sharer, so continue
        Cache *child = dynamic_cast<Cache *>(_children[child_i]); assert(child!=NULL);
        for (Addr line_addr_iter=line->addr; line_addr_iter<line->addr+get_line_size(); line_addr_iter += child->get_line_size())
        {
            if (line_addr_iter!=line->addr) {
                // also measure the latency for other line segments
                NVLOG1("%s\tline_writer_to_sharer 0x%lx +%ld cycles\n", this->_name.c_str(), line_addr_iter, _hit_latency);
                latency += _hit_latency;  // response from child to parent
                this->stats.writebacks_inc();
            }
            Line *child_line = child->addr2line_internal(line_addr_iter);
            if (!child_line) continue;
            child->line_writer_to_sharer(child_line, latency);
        }
    }
    if (!children_only) {
        this->line_data_writeback(line);
        line->state &= ~(LINE_MOD | LINE_EXC);
        line->state |= LINE_SHR;
    }
    NVLOG1("%s\t0x%lx\t new state %s sharers %lx\n",  _name.c_str(),  line->addr,  state2str(line->state).c_str(), line->sharers );
}

void
Cache :: line_rm(Line *line)
{
    NVLOG1("%s\tline_rm addr 0x%lx data 0x%lx\n", this->_name.c_str(), line->addr, (Addr)line->pdata);
    size_t direct_entry = this->addr2directentry(line->addr);
    this->_entries[direct_entry].erase(line);
}

void
Cache :: line_rm_recursive(Addr addr)
{
    Line *line = addr2line_internal(addr);
    if (line==NULL) return;
    NVLOG1("%s\tline_rm_recursive addr 0x%lx\n", this->_name.c_str(), addr);
    for (size_t child_i = 0; child_i<_children.size() && line->sharers>0; child_i++) {
        if (!bit(line->sharers, child_i)) continue; // a child is not a sharer, so continue
        Cache *child = dynamic_cast<Cache *>(_children[child_i]); assert(child!=NULL);
        for (Addr line_addr_iter=line->addr; line_addr_iter<line->addr+get_line_size(); line_addr_iter += child->get_line_size()) {
            child->line_rm_recursive(line_addr_iter);
        }
    }
#ifdef HAS_HTM
    if (pprocessor) {
        pprocessor->cb_line_evicted(line);
    }
#endif
    free(line->pdata);
    line->pdata = NULL;
    // remove in this cache as well
    this->line_rm(line);
}

void
Cache :: line_evict(Addr addr)
{
    Line *line = addr2line_internal(addr);
    if (line==NULL) return;
    this->line_evict(line);
}

void
Cache :: line_evict(Line *line)
{
    NVLOG1("%s\tline_evict addr 0x%lx data 0x%lx\n", this->_name.c_str(), line->addr, (Addr)line->pdata);
    // evict in all child caches
    for (size_t child_i = 0; child_i<_children.size() && line->sharers>0; child_i++) {
        if (!bit(line->sharers, child_i)) continue; // a child is not a sharer, so continue
        Cache *child = dynamic_cast<Cache *>(_children[child_i]); assert(child!=NULL);
        for (Addr line_addr_iter=line->addr; line_addr_iter<line->addr+get_line_size(); line_addr_iter += child->get_line_size()) {
            Line *child_line = child->addr2line_internal(line_addr_iter);
            if (child_line == NULL) continue;
            child->line_evict(child_line);
        }
    }
#ifdef HAS_HTM
    if (pprocessor) {
        pprocessor->cb_line_evicted(line);
    }
#endif
    if (!_parent_cache) {
	// notify the processor of the line removal
#ifdef HAS_HTM
	assert(pprocessor);
	pprocessor->cb_line_evicted(line);
#endif
    }
    this->line_data_writeback(line); // check if there is any data to writeback

    free(line->pdata);
    line->pdata = NULL;
    // remove in this cache as well
    this->line_rm(line);
}

Line *
Cache :: addr2line_internal(const Addr addr, const bool search_reverse)
{
    Addr line_addr = floor(addr, get_line_size());
    size_t direct_entry = this->addr2directentry(line_addr);
    if (search_reverse) {
        Line *line = _entries[direct_entry].get_no_reorder_reverse(line_addr);
        return line;
    } else {
        Line *line = _entries[direct_entry].get_no_reorder(line_addr);
        return line;
    }
}

Line *
Cache :: addr2line(const Addr addr, bool &set_overflow, Line *&overflow_line)
{
    Addr line_addr = floor(addr, this->get_line_size());
    size_t direct_entry = this->addr2directentry(line_addr);
    assert(_entries[direct_entry].size() <= _associativity);
    Line *line = _entries[direct_entry].get(line_addr, set_overflow, overflow_line);
    //  NVLOG1("%s\taddr2line>> addr 0x%lx data 0x%lx\n", this->_name.c_str(), line->addr, (Addr)line->pdata);
    assert(_entries[direct_entry].size() <= _associativity+1); // in case of an overflow there is one extra element
    return line;
}

bool
Cache :: is_reader(Addr addr)
{
    Line *line = this->addr2line_internal(addr);
    if (line==NULL) return false;
    if (line->state & (LINE_SHR | LINE_EXC | LINE_MOD)) return true;
    return false;
}

bool
Cache :: is_writer(Addr addr)
{
    Line *line = this->addr2line_internal(addr);
    if (line==NULL) return false;
    if (line->state & (LINE_EXC | LINE_MOD)) return true;
    return false;
}

void
Cache :: line_data_get_internal(const Addr addr, uint8_t *&pdata)
{
    // this function will likely be called to get the soon-to-be-evicted data
    // this data is probably at the end of the set deque
    // therefore we do a reverse search for the line (starting from the last element in the set)
    const bool search_reverse = true;
    Line *line = this->addr2line_internal(addr, search_reverse);
    if (line) { pdata = line->pdata; }
    pdata = NULL;
}

size_t
Cache :: get_num_valid_entries()
{
    tCacheEntries::iterator iter;
    size_t cnt_valid = 0;
    size_t idx = 0;
    for (iter=_entries.begin(); iter!=_entries.end(); iter++, idx++) {
        iter->rm_invalid_entries();
        cnt_valid += iter->size();
    }
    return cnt_valid;
}

inline size_t
Cache :: get_num_valid_entries(size_t direct_entry)
{
    _entries[direct_entry].rm_invalid_entries();
    return this->_entries[direct_entry].size();
}

void
Cache :: dump_stats(const char *description, std::ofstream *stats_file, size_t indentation)
{
    if (stats_file==NULL) {
        stats_file = get_stats_file();
    }
    if (description!=NULL) {
        *stats_file << nspaces(indentation).c_str() << "# " << description << "\n";
    }
    //*stats_file << this->_name.c_str() << " statistics dump.\n";
    this->stats.dump(*stats_file, this->_name.c_str(), indentation);

    childvec_t :: iterator citer;
    for (citer=_children.begin(); citer!=_children.end(); citer++) {
        (*citer)->dump_stats(NULL, stats_file, indentation+4);
    }
}

void
Cache :: line_get_as_modified(Addr addr)
{
    Line *line;
    Line *line_iter;
    line = line_iter = addr2line_internal(addr);
    // get line as modified (and invalidate all other line copies)
    for (Cache *cache_iter = this; cache_iter->_parent_cache; cache_iter = cache_iter->_parent_cache, line_iter = line_iter->parent_line)
    {
        NVLOG1("%s\tline_get_as_modified 0x%lx\n", cache_iter->_name.c_str(), addr);
        // make me a unique owner
        cache_iter->_parent_cache->line_make_owner_in_child_caches(line_iter->parent_line, cache_iter->_index_in_parent);
        line_iter->state |= LINE_EXC;
        line_iter->parent_line->state |= LINE_EXC;
        NVLOG1("%s\t0x%lx\t new state %s sharers %lx\n",  cache_iter->_name.c_str(),  line_iter->addr,  state2str(line_iter->state).c_str(), line_iter->sharers);
        NVLOG1("%s\t0x%lx\t new state %s sharers %lx\n",  cache_iter->_parent_cache->_name.c_str(),  line_iter->parent_line->addr,  state2str(line_iter->parent_line->state).c_str(), line_iter->parent_line->sharers );
    }
    line->state |= LINE_MOD;
    NVLOG1("%s\t0x%lx\t new state %s sharers %lx\n",  this->_name.c_str(),  line->addr,  state2str(line->state).c_str(), line->sharers);
}

// TODO test writethrough caches

void
Cache :: line_data_writeback(Line *line)
{
    if (line->pdata == NULL) return;
    assert(! ((line->state & (LINE_MOD | LINE_EXC)) && (line->state & LINE_TXW)) );
    if (!(line->state & (LINE_MOD | LINE_TXW))) return;
    if (_parent_cache && line->parent_line)
    {
        NVLOG1("%s\tline_data_writeback to %s 0x%lx data 0x%lx -> 0x%lx\n", this->_name.c_str(), _parent_cache->_name.c_str(), line->addr, (Addr)line->pdata, (Addr)line->parent_line->pdata);
        assert(line->parent_line->pdata != NULL);
        memcpy(line->parent_line->pdata+(line->addr - line->parent_line->addr), line->pdata, get_line_size());
        if (line->state & LINE_MOD) { line->parent_line->state |= LINE_MOD; } // propagate modified state
        NVLOG1("%s\t0x%lx\t new state %s sharers %lx\n",  _parent_cache->_name.c_str(),  line->parent_line->addr,  state2str(line->parent_line->state).c_str(), line->parent_line->sharers );
    }
    else
    {
        NVLOG1("%s\tline_data_writeback to main_mem 0x%lx data <- 0x%lx\n", this->_name.c_str(), line->addr, (Addr)line->pdata);
        // write the line data to the memory
//        Fault fault = rw_array_silent(line->addr, get_line_size(), line->pdata, true);
//        assert(fault == NoFault);
    }
}

void
Cache :: add_child(Cache *child) {
    // Add this cache to the list of child caches
    _children.add_child(child);
    if (_children.size()>1) {
        this->_is_private_cache = false;
    } else {
    }

    // Check whether the cache sizes are OK
    if (this->_associativity < child->_associativity) {
        fprintf(stderr, "CACHE WARNING: %s has lower associativity than its child %s (%ld < %ld)\n",
                this->_name.c_str(),
                child->_name.c_str(),
                this->_associativity,
                child->_associativity
               );
    }
    int my_lines = this->_associativity * this->_num_direct_entries;
    int child_lines = child->_associativity * child->_num_direct_entries;
    if (my_lines < child_lines) {
        fprintf(stderr, "CACHE WARNING: %s has lower capacity than its child %s (%d<%d)\n",
                this->_name.c_str(),
                child->_name.c_str(),
                my_lines,
                child_lines
               );
    }
    childvec_t :: iterator citer;
    int all_child_lines = 0;
    for (citer=_children.begin(); citer!=_children.end(); citer++) {
        Cache *child = *citer;
        assert(child != NULL);
        all_child_lines += child->_associativity * child->_num_direct_entries;
    }
    if (my_lines < all_child_lines) {
        fprintf(stderr, "CACHE WARNING: %s has lower capacity than ALL its children together (%d<%d)\n", this->_name.c_str(), my_lines, all_child_lines);
    }
}

std::ostream &
Cache::dump(std::ostream &os) {
    tCacheEntries::const_iterator citer;
    size_t cnt=0;
    for (citer=this->_entries.begin(); citer!=this->_entries.end(); citer++, cnt++) {
        if (this->get_num_valid_entries(cnt)==0) continue;
        os << _name.c_str() << " " << cnt << ": ";
        os << citer->str();
        os << std::endl;
    }
    return os;
}

void
Cache::reset_stats() {
    childvec_t :: const_iterator child_iter;
    for (child_iter=_children.begin(); child_iter!=_children.end(); ++child_iter) {
        (*child_iter)->reset_stats();
    }
    this->stats.reset();
}

void
Cache :: flush_data() {
    NVLOG1("%s\tflush_data\n", this->_name.c_str());
    tCacheEntries::iterator set_iter;
    my_cam::iterator set_iter2;
    for (set_iter=this->_entries.begin(); set_iter!=this->_entries.end(); ++set_iter) {
        for (set_iter2=set_iter->begin(); set_iter2!=set_iter->end(); ++set_iter2) {
            Line *line = *set_iter2;
            this->line_data_writeback(line);
            free(line->pdata);
            line->pdata = NULL;
        }
        set_iter->clear();
    }
}

void
Cache::reset() {
    childvec_t :: const_iterator child_iter;
    for (child_iter=_children.begin(); child_iter!=_children.end(); ++child_iter)
    {
        (*child_iter)->reset();
    }
    this->flush_data();
};

void
ChildMemories :: add_child(Cache *child)
{
    // built-in limit, for now!
    assert(this->size() <= 64);
    this->push_back(child);
}

bool
ChildMemories :: child_find_idx(Cache *child, int &child_index)
{
    int cnt = 0;
    childvec_t :: const_iterator citer;
    for(citer=this->begin(); citer!= this->end(); citer++, cnt++) {
        if (*citer == child) {
            child_index = cnt;
            return true;
        }
    }
    return false;
}

size_t
Cache :: addr2directentry(Addr addr)
{
    size_t line_bits = (size_t)log2power2(_line_size_bytes);
    size_t entry_bits = (size_t)log2power2(_num_direct_entries);
    return (size_t)bits(addr, line_bits, entry_bits);
}

std::ostream& operator << ( std::ostream &os, Cache &obj)
{
    return obj.dump(os);
}

