#include "sift_writer.h"
#include "sift_format.h"
#include "sift_utils.h"
#include "zfstream.h"

#include <cassert>
#include <cstring>
#include <iostream>

// Enable (>0) to print out everything we write
#define VERBOSE 0
#define VERBOSE_HEX 0

Sift::Writer::Writer(const char *filename, GetCodeFunc getCodeFunc, bool useCompression, const char *response_filename, uint32_t id, bool arch32)
   : response(NULL)
   , getCodeFunc(getCodeFunc)
   , ninstrs(0)
   , nbranch(0)
   , npredicate(0)
   , ninstrsmall(0)
   , ninstrext(0)
   , last_address(0)
   , icache()
   , m_id(id)
{
   memset(hsize, 0, sizeof(hsize));
   memset(haddr, 0, sizeof(haddr));

   m_response_filename = strdup(response_filename);

   uint64_t options = 0;
   if (useCompression)
      options |= CompressionZlib;
   if (arch32)
      options |= ArchIA32;

   output = new vofstream(filename, std::ios::out | std::ios::binary | std::ios::trunc);

   #if VERBOSE > 0
   std::cerr << "[DEBUG:" << m_id << "] Write Header" << std::endl;
   #endif

   Sift::Header hdr = { Sift::MagicNumber, 0 /* header size */, options, {}};
   output->write(reinterpret_cast<char*>(&hdr), sizeof(hdr));
   output->flush();

   if (options & CompressionZlib)
      output = new ozstream(output);
}

void Sift::Writer::End()
{
   #if VERBOSE > 0
   std::cerr << "[DEBUG:" << m_id << "] Write End" << std::endl;
   #endif

   if (output)
   {
      Record rec;
      rec.Other.zero = 0;
      rec.Other.type = RecOtherEnd;
      rec.Other.size = 0;
      output->write(reinterpret_cast<char*>(&rec), sizeof(rec.Other));
      output->flush();
   }

   if (response)
   {
/*
      // Disable EndResponse because of lock-up issues with Pin and sift_recorder
      #if VERBOSE > 0
      std::cerr << "[DEBUG:" << m_id << "] Write End - Response Wait" << std::endl;
      #endif

      Record respRec;
      response->read(reinterpret_cast<char*>(&respRec), sizeof(respRec.Other));
      assert(respRec.Other.zero == 0);
      assert(respRec.Other.type == RecOtherEndResponse);
*/
      delete response;
      response = NULL;
   }

   if (output)
   {
      delete output;
      output = NULL;
   }
}

Sift::Writer::~Writer()
{
   End();

   delete m_response_filename;

   #if VERBOSE > 3
   printf("instrs %lu hsize", ninstrs);
   for(int i = 1; i < 16; ++i)
      printf(" %u", hsize[i]);
   printf(" haddr");
   for(int i = 1; i <= MAX_DYNAMIC_ADDRESSES; ++i)
      printf(" %u", haddr[i]);
   printf(" branch %u predicate %u\n", nbranch, npredicate);
   printf("instrsmall %u ext %u\n", ninstrsmall, ninstrext);
   #endif
}

void Sift::Writer::Instruction(uint64_t addr, uint8_t size, uint8_t num_addresses, uint64_t addresses[], bool is_branch, bool taken, bool is_predicate, bool executed)
{
   assert(size < 16);
   assert(num_addresses <= MAX_DYNAMIC_ADDRESSES);

   // Send ICACHE record?
   for(uint64_t base_addr = addr & ICACHE_PAGE_MASK; base_addr <= ((addr + size - 1) & ICACHE_PAGE_MASK); base_addr += ICACHE_SIZE)
   {
      if (! icache[base_addr])
      {
         #if VERBOSE > 2
         std::cerr << "[DEBUG:" << m_id << "] Write icache" << std::endl;
         #endif
         Record rec;
         rec.Other.zero = 0;
         rec.Other.type = RecOtherIcache;
         rec.Other.size = sizeof(uint64_t) + ICACHE_SIZE;
         output->write(reinterpret_cast<char*>(&rec), sizeof(rec.Other));
         output->write(reinterpret_cast<char*>(&base_addr), sizeof(uint64_t));

         uint8_t buffer[ICACHE_SIZE];
         getCodeFunc(buffer, (const uint8_t *)base_addr, ICACHE_SIZE);
         output->write(reinterpret_cast<char*>(buffer), ICACHE_SIZE);

         icache[base_addr] = true;
      }
   }

   #if VERBOSE > 2
   printf("%016lx (%d) A%u %c%c %c%c\n", addr, size, num_addresses, is_branch?'B':'.', is_branch?(taken?'T':'.'):'.', is_predicate?'C':'.', is_predicate?(executed?'E':'n'):'.');
   #endif

   // Try as simple instruction
   if (addr == last_address && !is_predicate)
   {
      #if VERBOSE > 2
      std::cerr << "[DEBUG:" << m_id << "] Write Simple Instruction" << std::endl;
      #endif

      Record rec;
      rec.Instruction.size = size;
      rec.Instruction.num_addresses = num_addresses;
      rec.Instruction.is_branch = is_branch;
      rec.Instruction.taken = taken;
      output->write(reinterpret_cast<char*>(&rec), sizeof(rec.Instruction));

      #if VERBOSE_HEX > 2
      hexdump((char*)&rec, sizeof(rec.Instruction));
      #endif

      ninstrsmall++;
   }
   // Send as full instruction
   else
   {
      #if VERBOSE > 2
      std::cerr << "[DEBUG:" << m_id << "] Write Simple Full Instruction" << std::endl;
      #endif

      Record rec;
      memset(&rec, 0, sizeof(rec));
      rec.InstructionExt.type = 0;
      rec.InstructionExt.size = size;
      rec.InstructionExt.num_addresses = num_addresses;
      rec.InstructionExt.is_branch = is_branch;
      rec.InstructionExt.taken = taken;
      rec.InstructionExt.is_predicate = is_predicate;
      rec.InstructionExt.executed = executed;
      rec.InstructionExt.addr = addr;
      output->write(reinterpret_cast<char*>(&rec), sizeof(rec.InstructionExt));

      #if VERBOSE_HEX > 2
      hexdump((char*)&rec, sizeof(rec.InstructionExt));
      #endif

      last_address = addr;

      ninstrext++;
   }

   for(int i = 0; i < num_addresses; ++i)
      output->write(reinterpret_cast<char*>(&addresses[i]), sizeof(uint64_t));

   last_address += size;

   ninstrs++;
   hsize[size]++;
   haddr[num_addresses]++;
   if (is_branch)
      nbranch++;
   if (is_predicate)
      npredicate++;
}

void Sift::Writer::Output(uint8_t fd, const char *data, uint32_t size)
{
   #if VERBOSE > 1
   std::cerr << "[DEBUG:" << m_id << "] Write Output" << std::endl;
   #endif

   Record rec;
   rec.Other.zero = 0;
   rec.Other.type = RecOtherOutput;
   rec.Other.size = sizeof(uint8_t) + size;

   #if VERBOSE_HEX > 1
   hexdump((char*)&rec, sizeof(rec.Other));
   hexdump((char*)&fd, sizeof(fd));
   hexdump((char*)data, size);
   #endif

   output->write(reinterpret_cast<char*>(&rec), sizeof(rec.Other));
   output->write(reinterpret_cast<char*>(&fd), sizeof(uint8_t));
   output->write(data, size);
}

int32_t Sift::Writer::NewThread()
{
   #if VERBOSE > 0
   std::cerr << "[DEBUG:" << m_id << "] Write NewThread" << std::endl;
   #endif

   Record rec;
   rec.Other.zero = 0;
   rec.Other.type = RecOtherNewThread;
   rec.Other.size = 0;
   output->write(reinterpret_cast<char*>(&rec), sizeof(rec.Other));
   output->flush();
   #if VERBOSE > 0
   std::cerr << "[DEBUG:" << m_id << "] Write NewThread Done" << std::endl;
   #endif

   if (!response)
   {
     assert(strcmp(m_response_filename, "") != 0);
     response = new std::ifstream(m_response_filename, std::ios::in);
   }

   int32_t retcode = 0;
   while (true)
   {
      Record respRec;
      response->read(reinterpret_cast<char*>(&respRec), sizeof(rec.Other));
      assert(respRec.Other.zero == 0);

      switch(respRec.Other.type)
      {
         case RecOtherNewThreadResponse:
            #if VERBOSE > 0
            std::cerr << "[DEBUG:" << m_id << "] Read NewThreadResponse" << std::endl;
            #endif
            assert(respRec.Other.size == sizeof(retcode));
            response->read(reinterpret_cast<char*>(&retcode), sizeof(retcode));
            #if VERBOSE > 0
            std::cerr << "[DEBUG:" << m_id << "] Got NewThreadResponse thread=" << retcode << std::endl;
            #endif
            return retcode;
            break;
         default:
            assert(false);
            break;
      }
   }
   return -1;
}

uint64_t Sift::Writer::Syscall(uint16_t syscall_number, const char *data, uint32_t size)
{
   #if VERBOSE > 0
   std::cerr << "[DEBUG:" << m_id << "] Write Syscall" << std::endl;
   #endif

   Record rec;
   rec.Other.zero = 0;
   rec.Other.type = RecOtherSyscallRequest;
   rec.Other.size = sizeof(uint16_t) + size;
   #if VERBOSE_HEX > 0
   hexdump((char*)&rec, sizeof(rec.Other));
   hexdump((char*)&syscall_number, sizeof(syscall_number));
   hexdump((char*)data, size);
   #endif
   output->write(reinterpret_cast<char*>(&rec), sizeof(rec.Other));
   output->write(reinterpret_cast<char*>(&syscall_number), sizeof(uint16_t));
   output->write(data, size);
   output->flush();


   if (!response)
   {
     assert(strcmp(m_response_filename, "") != 0);
     response = new std::ifstream(m_response_filename, std::ios::in);
   }

   uint64_t retcode = 0;
   while (true)
   {
      Record respRec;
      response->read(reinterpret_cast<char*>(&respRec), sizeof(rec.Other));
      assert(respRec.Other.zero == 0);

      switch(respRec.Other.type)
      {
         case RecOtherSyscallResponse:
            #if VERBOSE > 0
            std::cerr << "[DEBUG:" << m_id << "] Read SyscallResponse" << std::endl;
            #endif
            assert(respRec.Other.size == sizeof(retcode));
            response->read(reinterpret_cast<char*>(&retcode), sizeof(retcode));
            return retcode;
            break;
         case RecOtherMemoryRequest:
            #if VERBOSE > 0
            std::cerr << "[DEBUG:" << m_id << "] Read MemoryRequest" << std::endl;
            #endif
            uint64_t addr;
            uint32_t size;
            MemoryLockType lock;
            MemoryOpType type;
            assert(respRec.Other.size >= (sizeof(addr)+sizeof(size)+sizeof(lock)+sizeof(type)));
            response->read(reinterpret_cast<char*>(&addr), sizeof(addr));
            response->read(reinterpret_cast<char*>(&size), sizeof(size));
            response->read(reinterpret_cast<char*>(&lock), sizeof(lock));
            response->read(reinterpret_cast<char*>(&type), sizeof(type));
            uint32_t payload_size = respRec.Other.size - (sizeof(addr)+sizeof(size)+sizeof(lock)+sizeof(type));
            assert(handleAccessMemoryFunc);
            if (type == MemRead)
            {
               assert(payload_size == 0);
               assert(size > 0);
               char *read_data = new char[size];
               bzero(read_data, size);
               // Do the read here via a callback to populate the read buffer
               handleAccessMemoryFunc(handleAccessMemoryArg, lock, type, addr, (uint8_t*)read_data, size);
               rec.Other.zero = 0;
               rec.Other.type = RecOtherMemoryResponse;
               rec.Other.size = sizeof(addr) + sizeof(type) + size;
               #if VEBOSE_HEX > 0
               hexdump((char*)&rec, sizeof(rec.Other));
               hexdump((char*)&addr, sizeof(addr));
               hexdump((char*)&type, sizeof(type));
               hexdump((char*)read_data, size);
               #endif
               #if VERBOSE
               std::cerr << "[DEBUG:" << m_id << "] Write AccessMemory-Read" << std::endl;
               #endif

               output->write(reinterpret_cast<char*>(&rec), sizeof(rec.Other));
               output->write(reinterpret_cast<char*>(&addr), sizeof(addr));
               output->write(reinterpret_cast<char*>(&type), sizeof(type));
               output->write(read_data, size);
               output->flush();
               delete read_data;
            }
            else if (type == MemWrite)
            {
               #if VERBOSE > 0
               std::cerr << "[DEBUG:" << m_id << "] Write AccessMemory-Write" << std::endl;
               #endif
               assert(payload_size > 0);
               assert(payload_size == size);
               char *payload = new char[payload_size];
               response->read(reinterpret_cast<char*>(payload), sizeof(payload_size));
               // Do the write here via a callback to write the data to the appropriate address
               handleAccessMemoryFunc(handleAccessMemoryArg, lock, type, addr, (uint8_t*)payload, payload_size);
               rec.Other.zero = 0;
               rec.Other.type = RecOtherMemoryResponse;
               rec.Other.size = sizeof(addr) + sizeof(type);
               #if VEBOSE_HEX > 0
               hexdump((char*)&rec, sizeof(rec.Other));
               hexdump((char*)&addr, sizeof(addr));
               hexdump((char*)&type, sizeof(type));
               #endif
               output->write(reinterpret_cast<char*>(&rec), sizeof(rec.Other));
               output->write(reinterpret_cast<char*>(&addr), sizeof(addr));
               output->write(reinterpret_cast<char*>(&type), sizeof(type));
               output->flush();
               delete payload;
            }
            else
            {
               assert(false);
            }
            break;
      }
   }

   // We should not get here
   return retcode;
}

int32_t Sift::Writer::Join(int32_t thread)
{
   #if VERBOSE > 0
   std::cerr << "[DEBUG:" << m_id << "] Write Join with thread=" << thread << std::endl;
   #endif

   Record rec;
   rec.Other.zero = 0;
   rec.Other.type = RecOtherJoin;
   rec.Other.size = sizeof(thread);
   output->write(reinterpret_cast<char*>(&rec), sizeof(rec.Other));
   output->write(reinterpret_cast<char*>(&thread), sizeof(thread));
   output->flush();
   #if VERBOSE > 0
   std::cerr << "[DEBUG:" << m_id << "] Write Join Done" << std::endl;
   #endif

   if (!response)
   {
     assert(strcmp(m_response_filename, "") != 0);
     response = new std::ifstream(m_response_filename, std::ios::in);
   }

   int32_t retcode = 0;
   while (true)
   {
      #if VERBOSE > 0
      std::cerr << "[DEBUG:" << m_id << "] Join Waiting for Response" << std::endl;
      #endif
      Record respRec;
      response->read(reinterpret_cast<char*>(&respRec), sizeof(rec.Other));
      assert(respRec.Other.zero == 0);

      switch(respRec.Other.type)
      {
         case RecOtherJoinResponse:
            #if VERBOSE > 0
            std::cerr << "[DEBUG:" << m_id << "] Read JoinResponse" << std::endl;
            #endif
            assert(respRec.Other.size == sizeof(retcode));
            response->read(reinterpret_cast<char*>(&retcode), sizeof(retcode));
            return retcode;
            break;
         default:
            assert(false);
            break;
      }
   }
   return -1;
}
