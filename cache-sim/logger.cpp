
#include "logger.h"
#include "globals.h"
#include "malloc.h"
#include <cstdio>

void Logger::Flush()
{
  //unsigned char *orig = this->allocPtr;
  // write the data out
  //W64 n;
  //WriteFile(this->logFile, this->buffer, (W64)(this->allocPtr - this->buffer), &n, NULL);
  fwrite(this->buffer, (size_t)(this->allocPtr - this->buffer), 1, this->logFile);
  this->allocPtr = this->buffer;
}

void Logger::Log(unsigned char *data, int size)
{
  unsigned char *orig = this->allocPtr;
  unsigned char *next = orig + size;
  while (size > 0)
  {
    if (next > &this->buffer[BUF_SIZE])
    {  // need to write the data out
        W64 n = (W64)(&this->buffer[BUF_SIZE] - orig);
        size -= n;
        while (n-- != 0)
        {
          *orig++ = *data++;
        }
	//WriteFile(this->logFile, this->buffer, BUF_SIZE, &n, NULL);
	fwrite(this->buffer, BUF_SIZE, 1, this->logFile);
        orig = this->buffer;
        next = orig + size;
    }
    else
    { // there's still some space in the buffer
        while (size-- != 0)
        {
          *orig++ = *data++;
        }
        break;
    }
  }
  this->allocPtr = next;
}

bool Logger::Init(const char *filename)
{
  //unsigned char *buf = (unsigned char*)VirtualAlloc(0, BUF_SIZE, MEM_COMMIT, PAGE_READWRITE);
  unsigned char *buf = (unsigned char *)malloc(BUF_SIZE);
  if (buf != NULL)
  {
    this->buffer = this->allocPtr = buf;
    //this->logFile = CreateFile(filename, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    this->logFile = fopen(filename, "wb");
    if (this->logFile==NULL) {
      fprintf(stderr, "URK!  logger could not create file '%s'\n", filename);
    }
    return true;
  }
  return false;
}

void Logger::Shutdown()
{
  W64 nBytes = (W64)(this->allocPtr - this->buffer);
  //WriteFile(this->logFile, this->buffer, nBytes, &nBytes, NULL);
  //CloseHandle(this->logFile);
  fwrite(this->buffer, nBytes, 1, this->logFile);
  fclose(this->logFile);
}
