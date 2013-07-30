#ifndef __CACHE_H__
#define __CACHE_H__

#define DEFAULT_ADDRESS_SPACE_SIZE 4*GB
#define DEFAULT_MAIN_MEMORY_ACCESS_TICKS 200
#define DEFAULT_CACHE_ACCESS_TICKS 10
#define DEFAULT_CACHELINE_SIZE_BYTES 64
#define DEFAULT_CACHE_ASSOCIATIVITY 4
#define DEFAULT_CACHE_DIRECT_ENTRIES 512
#define DEFAULT_COST_HIT 3

#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <utility>
#include <vector>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <ctime>
#include <malloc.h>
#include <deque>
#include "globals.h"
#ifdef HAS_HTM
  #include "proc_cache_interface.h"
#endif

struct Cache;
struct GenericMemory;

extern char *strbuf;
extern bool shutdown_started;
void nvlog_flush();

const uint8_t LINE_INV = 0;
const uint8_t LINE_SHR = 1<<0;
const uint8_t LINE_EXC = 1<<1; // line is EXCLUSIVE when there is no other line copy in the system, and it is NOT modified
const uint8_t LINE_MOD = 1<<2; // is EXCLUSIVE AND MODIFIED (dirty)
const uint8_t LINE_TXR = 1<<3;
const uint8_t LINE_TXW = 1<<4;

const std::string state2str (const uint8_t state);

struct Line
{
    Addr addr;
    uint8_t state;
    uint64_t sharers;
    uint8_t *pdata; // a pointer to the line data
    // a pointer to the same line in parent cache
    Line *parent_line;
    std::string str() const {
        std::ostringstream outputString;
        //outputString << (void*)this << ".0x" << std::hex << this->addr <<"."<< (void*)this->pdata << ":" << state2str(state) << "#" << this->sharers;
        outputString << "0x" << std::hex << this->addr << ":" << state2str(state) << "@" << this->sharers;
        return outputString.str();
    }
    Line(Addr addr) {
        this->addr = addr;
        this->state = LINE_INV;
        this->sharers = 0;
        this->pdata = NULL;
        this->parent_line = NULL;
    }
private:
    Line() {}
public:
};

// This CAM employs LRU replacement algorithm -> so we use deque underneath
struct my_cam : private std::deque<Line *>
{
    size_t __capacity;
    typedef std::deque<Line *>::iterator iterator;
    typedef std::deque<Line *>::const_iterator const_iterator;
    iterator begin() { return std::deque<Line *>::begin(); }
    iterator end() { return std::deque<Line *>::end(); }
    const_iterator begin() const { return std::deque<Line *>::begin(); }
    const_iterator end() const { return std::deque<Line *>::end(); }

    my_cam() : __capacity(0) { } // it won't work with capacity==0 !!!
    ~my_cam() { while (this->size() > 0) { if (this->back()->pdata) free(this->back()->pdata); delete this->back(); this->pop_back(); } }

    inline void set_capacity(size_t capacity) {
        __capacity = capacity;
    }

    inline std::string str() const {
        std::ostringstream outputString;
        outputString << "{ ";
        for (const_iterator cit = this->begin(); cit!= this->end(); ++cit) {
            outputString << (*cit)->str() << ", ";
        }
        outputString << "}";
        return outputString.str();
    }
    inline size_t size() { return std::deque<Line *>::size(); }
    inline Line * get_no_reorder(const Addr addr) {
	//NVLOG1("getting %lx w/o reordering\n", addr);
        for (const_iterator it = this->begin(); it!= this->end(); ++it) {
            if ((*it)->addr == addr)
                return *it;
        }
        return NULL;
    }
    inline Line *get_no_reorder_reverse(const Addr addr) {
        const const_reverse_iterator crend = this->rend();
        for (const_reverse_iterator it = this->rbegin(); it!= crend; ++it) {
            if ((*it)->addr == addr)
                return *it;
        }
        return NULL;
    }
    inline Line * get(const Addr addr, bool &overflow, Line *&overflow_elem) {
	//NVLOG1("getting %lx entries %ld/%ld %s\n", addr, this->size(), __capacity, this->str().c_str());
        overflow = false;
        overflow_elem = NULL;
        for (iterator it = this->begin(); it!= this->end(); ++it) {
            if ((*it)->addr == addr)
            { // ELEMENT RE-ACCESSING! move it to the end of the LRU
                if (it!=this->begin()) {
                    Line *tmp = *it;
                    std::deque<Line *>::erase(it);
                    this->push_front(tmp);
                }
		//NVLOG1("Found! entries %ld/%ld %s\n", this->size(), __capacity, this->str().c_str());
                return this->front();
            }
        }
        // if we got to here, the element WAS NOT FOUND!
	//NVLOG1("Adding! entries %ld/%ld %s\n", this->size(), __capacity, this->str().c_str());
        this->push_front(new Line(addr));
        // Maybe we have to remove one element?
        if (this->size() > __capacity)
        {
            overflow = true;
            overflow_elem = this->back(); // remove last element
	    //NVLOG1("overflow cap %ld/%ld: to remove %lx\n", this->size(), __capacity, overflow_elem->addr);
        }
        return this->front();
    }
    inline void clear() {
        while (this->size() > 0)
        {
            assert(this->back()->pdata == NULL);
            delete this->back();
            this->pop_back();
        }
        // deque.clear() also has linear complexity, so it's OK to do it this way
    }
    inline void erase(Line *to_rm) {
	//NVLOG1("removing %lx addr %lx data 0x%lx from %s ", (Addr)to_rm, to_rm->addr, (Addr)to_rm->pdata, this->str().c_str());
        for (iterator it = this->begin(); it!= this->end(); ++it)
        {
            //if ((*it)->addr == to_rm->addr) {
            if (*it == to_rm) {
                assert((*it)->pdata == NULL);
                delete *it;
                std::deque<Line *>::erase(it);
		//NVLOG1("removed!\n");
                return;
            }
        }
	//NVLOG1("not found!\n");
        }
        inline void rm_invalid_entries() {
            for (iterator it = this->begin(); it!= this->end();)
            {
                if ((*it)->state>LINE_INV) {
                    ++it;
                } else {
                    delete *it;
                    it = std::deque<Line *>::erase(it);
                }
            }
        }
        inline bool exists(const Addr addr) {
            for (iterator it = this->begin(); it!= this->end(); ++it)
            {
                if ((*it)->addr == addr) {
                    return true;
                }
            }
            return false;
        }
};

template <class T>
struct dbg_vector : std::vector<T>
{
    typedef typename std::vector<T>::const_iterator const_iterator;

    const char *c_str() {
        std::ostringstream outputString;
        outputString << "[ ";
        for (const_iterator cit = this->begin(); cit!= this->end(); cit++) {
            outputString << *cit << ", ";
        }
        outputString << " ]";
        return outputString.str().c_str();
    }
};

typedef dbg_vector<my_cam> tCacheEntries;
typedef GenericMemory *GenericMemoryPtr;

struct CacheStats
{
    size_t ticks;
    size_t hits;
    size_t hits_rd;
    size_t hits_wr;
    size_t misses;
    size_t misses_ld;
    size_t misses_st;
    size_t writebacks;

    CacheStats() :
	ticks(0), hits(0), hits_rd(0), hits_wr(0), misses(0), misses_ld(0), misses_st(0), writebacks(0)
    {};
    inline void reset() { ticks=0; hits=0; misses=0; }
    inline void ticks_inc(size_t cnt=1) { ticks+=cnt; }
    inline void hits_inc(size_t cnt=1) { hits+=cnt; }
    inline void hits_rd_inc(size_t cnt=1) { hits_rd+=cnt; }
    inline void hits_wr_inc(size_t cnt=1) { hits_wr+=cnt; }
    inline void misses_inc(size_t cnt=1) { misses+=cnt; }
    inline void misses_ld_inc(size_t cnt=1) { misses_ld+=cnt; }
    inline void misses_st_inc(size_t cnt=1) { misses_st+=cnt; }
    inline void writebacks_inc(size_t cnt=1) { writebacks+=cnt; }
    inline std::ostream & dump(std::ostream &os, const char *prefix, size_t indentation) {
        os << nspaces(indentation).c_str() << prefix << ":\n";
        os << nspaces(indentation+4).c_str() << "Ticks: " << this->ticks << std::endl;
        os << nspaces(indentation+4).c_str() << "Hits: " << this->hits << std::endl;
	os << nspaces(indentation+4).c_str() << "Hits Rd: " << this->hits_rd << std::endl;
	os << nspaces(indentation+4).c_str() << "Hits Wr: " << this->hits_wr << std::endl;
	os << nspaces(indentation+4).c_str() << "Misses: " << this->misses << std::endl;
        os << nspaces(indentation+4).c_str() << "Misses Load: " << this->misses_ld << std::endl;
        os << nspaces(indentation+4).c_str() << "Misses Store: " << this->misses_st << std::endl;
        os << nspaces(indentation+4).c_str() << "Writebacks: " << this->writebacks << std::endl;
        return os;
    }
};

struct GenericMemory
{
    std::ofstream *_stats_file;
    GenericMemory() : _stats_file(NULL) {}
    virtual ~GenericMemory() {};
    virtual void line_get(const Addr addr, const uint8_t line_state, size_t &latency, uint8_t *&pdata)=0;
    virtual void line_get_intercache(const Addr addr, const uint8_t line_state, size_t &latency, const unsigned child_index, Line *&parent_line)=0;
    virtual void line_evict(Addr addr)=0;
    virtual void line_evict(Line *line)=0;
    virtual void line_rm(Line *to_rm)=0;
    virtual void line_rm_recursive(Addr addr)=0;
    virtual int get_line_size()=0;
    virtual void add_child(Cache *child)=0;
    virtual void reset()=0;
    virtual void line_data_writeback(Line *line)=0;
    virtual void reset_stats() {};
    virtual void dump_stats(const char *description=NULL, std::ofstream *stats_file=NULL, size_t indentation=4)=0;
    std::ofstream *get_stats_file() {
        if (_stats_file==NULL) {
            _stats_file = new std::ofstream();
            _stats_file->open("stats_cache.txt", std::ios::out | std::ios::app | std::ios::binary);
            time_t rawtime; time (&rawtime);
            *_stats_file << "\n\n---\n";
            *_stats_file << "Execution: " << ctime (&rawtime) << "\nStatistics:\n";
        }
        return _stats_file;
    }
};

struct Cache;
typedef dbg_vector<Cache*> childvec_t;
struct ChildMemories : childvec_t
{
    Cache *operator[](size_t idx) { return childvec_t :: at(idx);  }
    void add_child(Cache *child);
    bool child_find_idx(Cache *child, int &child_index);
};

struct Cache : GenericMemory
{
    // instance name
    std::string _name;
    // where actual data is...
    tCacheEntries _entries;

    // some statistics
    CacheStats stats;

    // pointers to "parent" and "children"
    GenericMemoryPtr _parent;
    Cache *_parent_cache;
    ChildMemories _children;
    int _index_in_parent;

    // other cache parameters
    size_t _num_direct_entries;
    size_t _associativity;
    int _line_size_bytes;
    size_t _hit_latency;
    bool _is_private_cache;
    bool _is_writeback_cache;
#ifdef HAS_HTM
    // pointer to the processor
    CacheContainer *pprocessor;
#endif
    //FILE *nvlogfile;

    Cache (
            std::string name,
            GenericMemoryPtr parent_memory,
            const size_t num_direct_entries=DEFAULT_CACHE_DIRECT_ENTRIES,
            const size_t capacity=DEFAULT_CACHE_ASSOCIATIVITY,
            const int line_size_bytes=DEFAULT_CACHELINE_SIZE_BYTES,
            const size_t hit_latency=DEFAULT_CACHE_ACCESS_TICKS,
#ifdef HAS_HTM
            const bool is_writeback=true,
            CacheContainer *processor=NULL
#else
            const bool is_writeback=true
#endif
          ) :
        _name(name),
        _parent(parent_memory),
        _num_direct_entries(num_direct_entries),
        _associativity(capacity),
        _line_size_bytes(line_size_bytes),
        _hit_latency(hit_latency),
        _is_private_cache(true),
#ifdef HAS_HTM
        _is_writeback_cache(is_writeback),
        pprocessor(processor)
#else
        _is_writeback_cache(is_writeback)
#endif
        {
            // sanity checks: all these have to be a power of 2
            assert(is_power_of_2(num_direct_entries));
            assert(is_power_of_2(line_size_bytes));
            assert(is_power_of_2(capacity));
            assert(hit_latency>=0);
            // allocate all direct entries
            _entries.resize(num_direct_entries);
            for (size_t i=0; i<num_direct_entries; i++) {
                _entries[i].set_capacity(capacity);
            }
            _parent_cache = dynamic_cast<Cache *>(_parent);
            // connect to a parent memory
            _parent->add_child(this);
            if (_parent_cache) {
                // assert that a child has been connected
                bool child_found = _parent_cache->_children.child_find_idx(this, _index_in_parent);
                assert(true==child_found);
            }
        }
    virtual ~Cache() { shutdown_started = true; this->flush_data(); }

    virtual void line_get(const Addr addr, const uint8_t line_state, size_t &latency, uint8_t *&pdata);
    virtual void line_get_intercache(const Addr addr, const uint8_t line_state, size_t &latency, const unsigned child_index, Line *&parent_line);
    void line_data_get_internal(const Addr addr, uint8_t *&pdata);
    bool line_make_owner_in_child_caches(Line *line, unsigned child_index);
    virtual void line_evict(Addr addr);
    virtual void line_evict(Line *line);
    virtual void line_rm(Line *line);
    virtual void line_rm_recursive(Addr addr);
    inline virtual int get_line_size() { return _line_size_bytes; }
    inline bool is_line_present(const Addr addr) { return this->addr2line_internal(addr)!=NULL; }
    size_t get_num_valid_entries();
    size_t get_num_valid_entries(size_t direct_entry);
    virtual void add_child(Cache *child);
    size_t addr2directentry(Addr addr);
    void flush_data();
    virtual void reset();
    virtual void reset_stats();
    virtual void dump_stats(const char *description=NULL, std::ofstream *stats_file=NULL, size_t indentation=4);

    void line_data_writeback(Line *line);
    //  void line_data_writeback_2shared(Line *line);

    bool is_reader(Addr addr);
    bool is_writer(Addr addr);
    void line_writer_to_sharer(const Addr addr, size_t &latency, bool children_only=false);
    void line_writer_to_sharer(Line *line, size_t &latency, bool children_only=false);
    void line_get_as_modified(Addr addr);
    void line_mark_in_parent(Addr addr, uint8_t line_state_req, size_t &latency);

    Line *addr2line_internal(const Addr addr, const bool search_reverse=false);
    Line *addr2line(const Addr addr, bool &overflow, Line *&overflow_line);
    // content printing (dumping) support
    std::ostream & dump(std::ostream &os);
    friend std::ostream & operator<<(std::ostream &cout, Cache &obj);
};

struct MainMemory : GenericMemory
{
	Addr _address_space_size;
	size_t _hit_latency_read;
	size_t _hit_latency_write;
	ChildMemories _children;
	CacheStats stats;
	MainMemory(
			Addr address_space_size=DEFAULT_ADDRESS_SPACE_SIZE,
			size_t hit_latency_read=DEFAULT_MAIN_MEMORY_ACCESS_TICKS,
			size_t hit_latency_write=DEFAULT_MAIN_MEMORY_ACCESS_TICKS
			) :
		_address_space_size(address_space_size),
		_hit_latency_read(hit_latency_read),
		_hit_latency_write(hit_latency_write)
	{
		assert(is_power_of_2(address_space_size));
		assert(hit_latency_read>=0);
		assert(hit_latency_write>=0);
	}
	virtual void line_get(const Addr addr, const uint8_t line_state_req, size_t &latency, uint8_t *&pdata)
	{
		if (line_state_req==LINE_SHR) {
			latency += _hit_latency_read;
			stats.ticks_inc(_hit_latency_read);
			stats.hits_rd_inc();
		} else if (line_state_req == LINE_MOD || line_state_req == LINE_EXC) {
			latency += _hit_latency_write;
			stats.ticks_inc(_hit_latency_write);
			stats.hits_wr_inc();
		} else {
			std::cerr << (void *)addr << " request for state: " << state2str(line_state_req) << "\n";
			assert(false && "Unhandled line_state_req");
		}
		stats.hits_inc();
	}
	virtual void line_get_intercache(
			const Addr addr,
			const uint8_t line_state_req,
			size_t &latency,
			const unsigned child_index,
			Line *&parent_line)
	{
		if (line_state_req==LINE_SHR) {
			latency += _hit_latency_read;
			stats.ticks_inc(_hit_latency_read);
			stats.hits_rd_inc();
		} else if (line_state_req == LINE_MOD || line_state_req == LINE_EXC) {
			latency += _hit_latency_write;
			stats.ticks_inc(_hit_latency_write);
			stats.hits_wr_inc();
		} else {
			std::cerr << (void *)addr << " request for state: " << state2str(line_state_req) << "\n";
			assert(false && "Unhandled line_state_req");
		}
		stats.hits_inc();
	}
	virtual void line_evict(Addr addr) {assert(false);}
	virtual void line_evict(Line *line) {assert(false);}
	virtual void line_rm(Line *line) {assert(false);}
	virtual void line_rm_recursive(Addr addr) {assert(false);}
	virtual int get_line_size() {assert(false); return 0;}

	void addr_range_writer_to_sharer(const Addr addr, const unsigned bytes) {
		childvec_t :: iterator citer;
		size_t latency = 0; // which we'll throw away
		for (citer=_children.begin(); citer!=_children.end(); citer++) {
			for (Addr line_addr_iter=addr; line_addr_iter<addr+bytes; line_addr_iter += (*citer)->get_line_size())
			{
				// this will fail if child is not a cache!
				Line *line = (*citer)->addr2line_internal(line_addr_iter);
				if (line) {
					(*citer)->line_writer_to_sharer(line, latency);
				}
			}
		}
	}
	void addr_range_invalidate(const Addr addr, const unsigned bytes) {
		childvec_t :: iterator citer;
		for (citer=_children.begin(); citer!=_children.end(); citer++) {
			for (Addr line_addr_iter=addr; line_addr_iter<addr+bytes; line_addr_iter += (*citer)->get_line_size())
			{
				// this will fail if child is not a cache!
				(*citer)->line_evict(line_addr_iter);
				// we have to evict (and writeback) because the simulator might modify
				// only a part of the line
				// this way (if we write-back current data), when we get the same
				// line next time, we will get our data back
			}
		}
	}
	virtual void reset_stats() {
		childvec_t :: iterator citer;
		for (citer=_children.begin(); citer!=_children.end(); citer++) {
			(*citer)->reset_stats();
		}
		stats.reset();
	};
	virtual void reset() {
		childvec_t :: iterator citer;
		for (citer=_children.begin(); citer!=_children.end(); citer++) {
			(*citer)->reset();
		}
	};
	virtual void dump_stats(const char *description=NULL, std::ofstream *stats_file=NULL, size_t indentation=4) {
		if (stats_file==NULL) {
			stats_file = get_stats_file();
		}
		*stats_file << "\n";
		if (description!=NULL) {
			*stats_file << nspaces(indentation).c_str() << "# " << description << "\n";
		}
		//*stats_file << this->_name.c_str() << " statistics dump.\n";
		this->stats.dump(*stats_file, "- PCM", indentation);

		childvec_t :: const_iterator citer;
		for (citer=_children.begin(); citer!=_children.end(); citer++) {
			(*citer)->dump_stats(NULL, stats_file, indentation+4);
		}
	}
	//  virtual size_t get_num_valid_entries() {return MIN2((size_t)-1, (size_t)_address_space_size);};
	virtual void line_data_writeback(Line *line) {
		stats.writebacks_inc();
		stats.ticks_inc(_hit_latency_write);
	}
	virtual void add_child(Cache *child) {
		_children.add_child(child);
	};
};


#endif //__CACHE_H__

