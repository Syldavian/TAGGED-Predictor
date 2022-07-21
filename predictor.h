#ifndef _PREDICTOR_H_
#define _PREDICTOR_H_

#include "utils.h"
#include "tracer.h"
#include <bitset>


/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

struct Entry {
  UINT32 counter; //3 bits
  UINT32 tag; //15 bits
  UINT32 useful; //2 bits
};

struct CompressedHistory {
  UINT32 compressed_history; //15 bits
	UINT32 compressed_length; //4 bits
	UINT32 original_length; //max 10 bits
};

struct Tage {
  Entry entries[1024];
  UINT32 history_length; //max 10 bits
  UINT32 tag_width; //4 bits
  CompressedHistory compressed_index;
  CompressedHistory compressed_tag[2];
  UINT32 tage_index; //10 bits
  UINT32 tage_tag; //15 bits
};

class PREDICTOR{

  // The state is defined for Gshare, change for your design

 private:
  bitset<641> global_hist; //641 bits
  unsigned long long path_hist; //32 bits

  bool base_pred; //1 bit
  bool provider_pred; //1 bit
  bool alt_pred;//1 bit
  int provider_table;//4 bits
  int alt_table;//4 bits
  UINT32 use_alt;//4 bits

  UINT32 *base_predictor;//2^13 * 2 bits/counter = 2KB
  UINT32 base_len;//4 bits

  Tage tage[11];

  UINT32 clock;//18 bits
  bool flip;//1 bit

 public:

  // The interface to the four functions below CAN NOT be changed

  PREDICTOR(void);
  bool    GetPrediction(UINT32 PC);  
  void    UpdatePredictor(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget);
  void    TrackOtherInst(UINT32 PC, OpType opType, UINT32 branchTarget);

  // Contestants can define their own functions below
  void    Compress(CompressedHistory *h);

};



/***********************************************************/
#endif

