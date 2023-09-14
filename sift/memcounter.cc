#define __STDC_FORMAT_MACROS

#include "sift_reader.h"

#include <inttypes.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <map>
#include <unordered_map>

extern "C" {
#include "xed-decode.h"
#include "xed-state.h"
#include "xed-decoded-inst-api.h"
}

unsigned long long num_mem_acc = 0;
unsigned long long num_mem_read = 0;
unsigned long long num_mem_write = 0;

#if PIN_REV >= 67254
extern "C" {
//#include "xed-decoded-inst-api.h"
}
#endif

int main(int argc, char* argv[])
{
   if (argc > 1 && strcmp(argv[1], "-d") == 0)
   {
      Sift::Reader reader(argv[2]);
      //const xed_syntax_enum_t syntax = XED_SYNTAX_ATT;

      uint64_t icount = 0;
      std::map<uint64_t, const Sift::StaticInstruction*> instructions;
      std::unordered_map<uint64_t, uint64_t> icounts;

      Sift::Instruction inst;
      while(reader.Read(inst))
      {
         if ((icount++ & 0xffff) == 0)
            fprintf(stderr, "Reading SIFT trace: %" PRId64 "%%\r", 100 * reader.getPosition() / reader.getLength());

         instructions[inst.sinst->addr] = inst.sinst;
         icounts[inst.sinst->addr]++;
      }
      fprintf(stderr, "                                       \r");

      uint64_t eip_last = 0;
      for(auto it = instructions.begin(); it != instructions.end(); ++it)
      {
         if (eip_last && (it->first != eip_last))
            printf("\n");
         printf("%12" PRId64 "   ", icounts[it->first]);
         printf("%16" PRIx64 ":   ", it->first);
         for(int i = 0; i < (it->second->size < 8 ? it->second->size : 8); ++i)
            printf("%02x ", it->second->data[i]);
         for(int i = it->second->size; i < 8; ++i)
            printf("   ");
         char buffer[64] = {0};
#if PIN_REV >= 67254
         //xed_format_context(syntax, &it->second->xed_inst, buffer, sizeof(buffer) - 1, it->first, 0, 0);
#else
         //xed_format(syntax, &it->second->xed_inst, buffer, sizeof(buffer) - 1, it->first);
#endif
         printf("  %-40s\n", buffer);
         if (it->second->size > 8)
         {
            printf("                                   ");
            for(int i = 8; i < it->second->size; ++i)
               printf("%02x ", it->second->data[i]);
            printf("\n");
         }
         eip_last = it->first + it->second->size;
      }
   }
   else if (argc > 1)
   {
      Sift::Reader* readerptr = NULL;
      if (argc > 2 && strcmp(argv[2], "-r") == 0) {
         readerptr = new Sift::Reader(argv[1], argv[3]);
      } else {
         readerptr = new Sift::Reader(argv[1]);
      }
      //const xed_syntax_enum_t syntax = XED_SYNTAX_ATT;
      Sift::Reader& reader = *readerptr;
      Sift::Instruction inst;
      while(reader.Read(inst))
      {
         
         // We only rely on xed lib to decode the instruction, since there are some conflicts on num_memop
         // between xed and sniper.Inst. 
         xed_decoded_inst_t xedd;
         xed_decoded_inst_zero(&xedd);
         xed_error_enum_t err;
         unsigned int num_memop;
         // xed_state_t xed_state = {XED_MACHINE_MODE_LONG_64, XED_ADDRESS_WIDTH_64b};
         // xed_decoded_inst_zero_set_mode(&xedd, &xed_state);

         if ((err = xed_decode(&xedd, inst.sinst->data, inst.sinst->size)) == XED_ERROR_NONE) {
            if (!xed_decoded_inst_get_attribute(&xedd, XED_ATTRIBUTE_NOP) && (num_memop = xed_decoded_inst_number_of_memory_operands(&xedd)) > 0) {

               printf("%016" PRIx64 " ", inst.sinst->addr);
               char buffer[64] = {0};
               printf("%-40s  ", buffer);
               printf("\n");
               printf("                 -- addr");

               num_mem_acc += num_memop;
               bool flag_read = false, flag_write = false;
               for(uint32_t i = 0; i < num_memop; ++i) {
                  printf(" %08" PRIx64, inst.addresses[i]);
                  if (xed_decoded_inst_mem_read(&xedd, i)) {
                     flag_read = true;
                  }
                  if (xed_decoded_inst_mem_written(&xedd, i)) {
                     flag_write = true;
                  }
               }
               num_mem_read += flag_read;
               num_mem_write += flag_write;

               printf(" -- r%d w%d", flag_read, flag_write);
               printf("\n");
            }
         }
      }
      fprintf(stderr, "num_mem_acc: %llu num_mem_read: %llu num_mem_write: %llu\n", num_mem_acc, num_mem_read, num_mem_write);
      delete readerptr; 
   }
   else
   {
      printf("Usage: %s [-d] <file.sift>\n", argv[0]);
   }
}
