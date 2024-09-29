#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <random>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <unistd.h>
#include <vector>

unsigned char bss;
unsigned char *bss_addr = nullptr;

template <bool B = true>
void
leak(const unsigned long long sz)
{
  constexpr unsigned long long tag = 16;     // size of magic tag in n bytes
  unsigned long long total = tag + sz;       // total to allocate
  // allocate memory
  unsigned char *memory = new unsigned char[total](0);     // or reinterpret_cast<unsigned char *>(malloc(total));
  if constexpr ( B )
    std::cout << "Allocated at: " << (int *)memory << std::endl;

  // tag the memory with a unique identifier (or use known data within the region)
  memory[0] = 0xFF;
  memory[1] = 0xAA;
  memory[2] = 0x11;
  memory[3] = 0x22;
  memory[4] = 0x55;
  memory[5] = 0x22;
  memory[6] = 0x11;
  memory[7] = 0x00;
  // the tag should be unique enough as to not generate false positives when
  // searching. although no tag permutation is immune to false hits, it should
  // be long and _not random_ enough as to have the probability approach zero

  // memory is now leaked
  // notice, no free()/delete[]
  // for sake of brevity let's explicitly zero out the stack pointer
  memory = nullptr;
}

struct mmap_page {
  void *s;
  void *e;
};

std::vector<mmap_page>
get_pages()
{
  // open the /proc/self/mem file which points to our current process and seek the heap. (mallocd memory is always heap
  // tagged)
  std::vector<mmap_page> pages;

  const char *maps_file = "/proc/self/maps";
  FILE *file = std::fopen(maps_file, "r");
  if ( !file )
    return pages;

  char line[256];
  while ( std::fgets(line, sizeof(line), file) ) {
    void *start = nullptr;
    void *end = nullptr;
    char perms[5];
    char path[256] = "";
    if ( std::sscanf(line, "%p-%p %4s %*s %*s %*s %255s", &start, &end, perms, path) >= 4 ) {
      if ( std::strstr(path, "[heap]") && std::strstr(perms, "r") ) {
        pages.emplace_back(start, end);
      }
    }
  }
  std::fclose(file);
  return pages;
}

template <bool B = true>
inline void
cleanup_annotate(void)
{
  if constexpr ( B )
    std::cout << "Pulling list of mmap'd pages from /proc/self/mem" << std::endl;
  auto pgs = get_pages();
  if constexpr ( B )
    std::cout << "Found " << pgs.size() << " pages." << std::endl;
  if constexpr ( B )
    std::cout << "Searching for magic tag." << std::endl;
  // 0xFF 0xAA 0x11 0x22 0x55 0x22 0x11 0x00 [size encoding]
  for ( auto &n : pgs ) {
    if constexpr ( B )
      std::cout << "Start: " << n.s << std::endl;
    if constexpr ( B )
      std::cout << "End:" << n.e << std::endl;
    unsigned char *ptr = reinterpret_cast<unsigned char *>(n.s);
    unsigned char *end = reinterpret_cast<unsigned char *>(n.e);
    while ( ptr < end && ptr + 8 < end ) {
      if ( *ptr == 0xFF and *(ptr + 1) == 0xAA and *(ptr + 2) == 0x11 and *(ptr + 3) == 0x22 and *(ptr + 4) == 0x55
           and *(ptr + 5) == 0x22 and *(ptr + 6) == 0x11 and *(ptr + 7) == 0x00 ) {
        if constexpr ( B ) {
          std::cout << "Found at: " << (int *)ptr << std::endl;
          // should be identical with the address printed by leak()
          std::cout << std::hex << std::uppercase;
          std::cout << (int)*(ptr + 0) << " ";
          std::cout << (int)*(ptr + 1) << " ";
          std::cout << (int)*(ptr + 2) << " ";
          std::cout << (int)*(ptr + 3) << " ";
          std::cout << (int)*(ptr + 4) << " ";
          std::cout << (int)*(ptr + 5) << " ";
          std::cout << (int)*(ptr + 6) << " ";
          std::cout << (int)*(ptr + 7) << std::endl;
          std::cout << std::dec << std::nouppercase;
          std::cout << "Freeing memory" << std::endl;
        }
        delete[] ptr;     // executes correctly, no double free or mem corruption errors
        // memleak tools & static checkers will STILL recognize this as a 'leak' even though the memory is gone
        // furthermore it's technically possible to also intervene and release heap pages directly via munmap, although
        // doing so MANDATES all allocations are handled by your program directly. (meaning malloc/free/regular
        // allocators become unusable since you can't free memory arbitrarily under them)
        if constexpr ( B )
          std::cout << "The memory has been freed" << std::endl;

        break;
      }
      ptr += 8;
    }
  }
}
int chn = 0;
inline void
cleanup(void)
{
  auto pgs = get_pages();
  // 0xFF 0xAA 0x11 0x22 0x55 0x22 0x11 0x00 [size encoding]
  for ( auto &n : pgs ) {
    unsigned char *ptr = reinterpret_cast<unsigned char *>(n.s);
    unsigned char *end = reinterpret_cast<unsigned char *>(n.e);
    while ( ptr < end && ptr + 8 < end ) {
      if ( *ptr == 0xFF and *(ptr + 1) == 0xAA and *(ptr + 2) == 0x11 and *(ptr + 3) == 0x22 and *(ptr + 4) == 0x55
           and *(ptr + 5) == 0x22 and *(ptr + 6) == 0x11 and *(ptr + 7) == 0x00 ) {
        delete[] ptr;
        std::cout << "Freed memory chunk nr. " << chn++ << std::endl;
        if ( chn >= 1999 )
          break;     // for simplicity
      }

      ptr += 8;
    }
  }
}
inline void
inf(void)
{
  volatile int x = 0;
  while ( true ) {
    leak<false>(65536);
    cleanup_annotate<false>();
    x++;
  }
}

int
main(void)
{
  // uncomment and see that memory usage isn't exploding
  // inf(); return 0;
  const size_t sz = 8192;
  std::cout << "Allocating " << sz << " elements and subsequently leaking it." << std::endl;
  leak<true>(sz);
  // memory* is now irrevocably lost (permanently leaked), let's retrieve it
  cleanup_annotate<true>();
  // commence hard test to prove it's not a fluke
  std::cout << "Stress test:" << std::endl;
  std::cout << "(press any key to continue)" << std::endl;
  getchar();
  std::random_device rd;
  std::mt19937_64 gen(rd());
  std::uniform_int_distribution<unsigned long long> dis(2156, 74563);
  for ( auto i = 0; i < 2000; i++ )
    leak(dis(gen));
  cleanup();
  return 0;
}
