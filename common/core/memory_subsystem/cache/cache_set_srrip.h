#ifndef CACHE_SET_SRRIP_H
#define CACHE_SET_SRRIP_H

#include "cache_set.h"

class CacheSetSRRIP : public CacheSet
{
   public:
      CacheSetSRRIP(String cfgname, core_id_t core_id,
            CacheBase::cache_t cache_type,
            UInt32 associativity, UInt32 blocksize);
      ~CacheSetSRRIP();

      UInt32 getReplacementIndex(CacheCntlr *cntlr);
      void updateReplacementIndex(UInt32 accessed_index);

   private:
      const UInt8 m_rrip_numbits;
      const UInt8 m_rrip_max;
      const UInt8 m_rrip_insert;
      UInt8* m_rrip_bits;
      UInt8  m_replacement_pointer;
};

#endif /* CACHE_SET_H */