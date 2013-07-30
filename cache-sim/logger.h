
#ifndef __NVLOGGER_H__
#define __NVLOGGER_H__

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#ifndef NO_STRICT
#ifndef STRICT
#define STRICT 1
#endif
#endif /* NO_STRICT */
#define VC_EXTRALEAN
//#include <windows.h>
//#define HANDLE FILE *
//#define CRITICAL_SECTION static pthread_mutex_t

struct Logger
{
private:
   const int BUF_SIZE;
   unsigned char *allocPtr;
   unsigned char *buffer;
   FILE * logFile;
   //CRITICAL_SECTION csLogger;

public:
   Logger (const char *filename, int buf_size = 4*4096) : BUF_SIZE(buf_size)
   {
     assert(this->Init(filename));
   }

   bool Init(const char *filename);
   void Shutdown();
   void Flush();
   void Log(unsigned char *data, int size);
};


#endif // __NVLOGGER_H__
