
#include <execinfo.h>
#include <csignal>
#include <cxxabi.h>

std::string
demangle(const char* symbol) {
  size_t size;
  int status;
  char temp[128];
  char* result;
  //first, try to demangle a c++ name
  if (1 == sscanf(symbol, "%*[^'(']%*[^'_']%[^')''+']", temp)) {
    if (NULL != (result = abi::__cxa_demangle(temp, NULL, &size, &status))) {
      return result;
    }
  }
  //if that didn't work, try to get a regular c symbol
  if (1 == sscanf(symbol, "%127s", temp)) {
    return temp;
  }
 
  //if all else fails, just return the symbol
  return symbol;
}

void print_backtrace (int) {
  // prevent infinite recursion if print_backtrace() causes another segfault
  std::signal(SIGSEGV, SIG_DFL);
  void *array[100];
  uint64_t size;
  char **strings;
  uint64_t i;

  size = backtrace (array, 100);
  strings = backtrace_symbols (array, size);

  fprintf(stderr, "Stack frames (%zd):\n", size-2); // first two are: print_backtrace and sigfaultHandler

  for (i = 2; i < size; i++)
    fprintf(stderr, "%s\n", demangle(strings[i]).c_str());

  free (strings);
  std::abort();
}

// void
// segfaultHandler(int sigtype)
// {
//     log_flush();
//     print_backtrace(sigtype);
//     exit(-1);
// }
// int main()
// {
//   signal(SIGSEGV, segfaultHandler);
// }
