#include "hit_where.h"

const char * HitWhereString(HitWhere::where_t where) {
   switch(where)
   {
      case HitWhere::L1I:             return "L1I";
      case HitWhere::L1_OWN:          return "L1";
      case HitWhere::L2_OWN:          return "L2";
      case HitWhere::L3_OWN:          return "L3";
      case HitWhere::L4_OWN:          return "L4";
      case HitWhere::L1_SIBLING:      return "L1_S";
      case HitWhere::L2_SIBLING:      return "L2_S";
      case HitWhere::L3_SIBLING:      return "L3_S";
      case HitWhere::L4_SIBLING:      return "L4_S";
      case HitWhere::MISS:            return "miss";
      case HitWhere::DRAM_LOCAL:      return "dram-local";
      case HitWhere::DRAM_REMOTE:     return "dram-remote";
      case HitWhere::CACHE_REMOTE:    return "cache-remote";
      case HitWhere::UNKNOWN:         return "unknown";
      case HitWhere::PREDICATE_FALSE: return "predicate-false";
      default:                        return "????";
   }
}
