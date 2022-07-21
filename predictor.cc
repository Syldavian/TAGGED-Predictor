#include "predictor.h"


#define PHT_CTR_MAX  3
#define PHT_CTR_INIT 2

#define HIST_LEN   13

/////////////// STORAGE BUDGET JUSTIFICATION ////////////////
// Total storage budget: 32KB + 17 bits
// Total PHT counters: 2^17 
// Total PHT size = 2^17 * 2 bits/counter = 2^18 bits = 32KB
// GHR size: 17 bits
// Total Size = PHT size + GHR size
/////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

PREDICTOR::PREDICTOR(void){
  
  base_len    = (1<< HIST_LEN);
  base_predictor = new UINT32[base_len];
  for (UINT32 ii=0; ii< base_len; ii++){
    base_predictor[ii]=PHT_CTR_INIT; 
  }

  tage[0].history_length = 640;
  tage[1].history_length = 403;
  tage[2].history_length = 254;
  tage[3].history_length = 160;
  tage[4].history_length = 101;
  tage[5].history_length = 64;
  tage[6].history_length = 40;
  tage[7].history_length = 25;
  tage[8].history_length = 16;
  tage[9].history_length = 10;
  tage[10].history_length = 6;

  for (int i=0; i<10; i++){
    tage[i].tag_width = 15; 
  }
  tage[10].tag_width = 14;

  for (int i=0; i<11; i++){
    tage[i].compressed_index.compressed_history = 0;
    tage[i].compressed_index.original_length = tage[i].history_length;
    tage[i].compressed_index.compressed_length = 10;
    tage[i].compressed_tag[0].compressed_history = 0;
    tage[i].compressed_tag[0].original_length = tage[i].history_length;
    tage[i].compressed_tag[0].compressed_length = tage[i].tag_width;
    tage[i].compressed_tag[1].compressed_history = 0;
    tage[i].compressed_tag[1].original_length = tage[i].history_length;
    tage[i].compressed_tag[1].compressed_length = tage[i].tag_width-1;
    for (int j=0; j<1024; j++){
      tage[i].entries[j].counter = 0;
      tage[i].entries[j].tag = 0;
      tage[i].entries[j].useful = 0;
    }
    tage[i].tage_index = 0;
    tage[i].tage_tag = 0;
  }

  clock = 0;
  flip = true;
  global_hist.reset();
  path_hist = 0;
  use_alt = 8;

}

/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

bool   PREDICTOR::GetPrediction(UINT32 PC){

  provider_pred = true;
  provider_table = 11;
  alt_pred = true;
  alt_table = 11;

  for(int i=0; i<11; i++){
    int tag = PC ^ tage[i].compressed_tag[0].compressed_history ^ (tage[i].compressed_tag[1].compressed_history << 1);
    tage[i].tage_tag = tag & ((1 << tage[i].tag_width) - 1);
    int index = PC ^ (PC >> (10-i)) ^ tage[i].compressed_index.compressed_history ^ (path_hist >> (10-i));
    tage[i].tage_index = index & ((1 << 10) - 1);
  }
  for (int i=0; i<11; i++){
    if (tage[i].entries[tage[i].tage_index].tag == tage[i].tage_tag){
      provider_table = i;
      break;
    }
  }
  for (int i=provider_table+1; i<11; i++){
    if (tage[i].entries[tage[i].tage_index].tag == tage[i].tage_tag){
      alt_table = i;
      break;
    }
  }

  int index = (PC & ((1 << HIST_LEN) - 1));
  UINT32 phtCounter = base_predictor[index];
  if (phtCounter > PHT_CTR_MAX/2){
    base_pred = TAKEN; 
  }else{
    base_pred = NOT_TAKEN; 
  }

  if (provider_table < 11){
    if (alt_table < 11){
      if (tage[alt_table].entries[tage[alt_table].tage_index].counter >= 7/2)
        alt_pred = TAKEN;
      else
        alt_pred = NOT_TAKEN;
    }
    else{
      alt_pred = base_pred;
    }

    if ((tage[provider_table].entries[tage[provider_table].tage_index].counter != 3) ||
        (tage[provider_table].entries[tage[provider_table].tage_index].counter != 4) ||
        (tage[provider_table].entries[tage[provider_table].tage_index].useful != 0) ||
        (use_alt < 8)){
      if (tage[provider_table].entries[tage[provider_table].tage_index].counter > 7/2)
        provider_pred = TAKEN;
      else
        provider_pred = NOT_TAKEN;
      return provider_pred;
    }
    else{
      return alt_pred;
    }  
  }
  else{
    return base_pred;
  }
  
}


/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

void  PREDICTOR::UpdatePredictor(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget){

  if ((provider_table>0) && (predDir!=resolveDir)){
    bool found = false;
    for (int i=0; i<provider_table; i++){
      if (tage[i].entries[tage[i].tage_index].useful == 0){
        found = true;
        break;
      }
    }

    int r = rand();
    if (!found){
      int random_table = r % 11;
      if (resolveDir == TAKEN)
        tage[random_table].entries[tage[random_table].tage_index].counter = 4;
      else
        tage[random_table].entries[tage[random_table].tage_index].counter = 3;
      tage[random_table].entries[tage[random_table].tage_index].useful = 0;
      tage[random_table].entries[tage[random_table].tage_index].tag = tage[random_table].tage_tag;
    }
    else{
      int random = r % 100;
      for (int i=provider_table-1; i>=0; i--){
        if (tage[i].entries[tage[i].tage_index].useful == 0){
          if (random <= 99/(provider_table-i+1)){
            if (resolveDir == TAKEN)
              tage[i].entries[tage[i].tage_index].counter = 4;
            else
              tage[i].entries[tage[i].tage_index].counter = 3;
            tage[i].entries[tage[i].tage_index].useful = 0;
            tage[i].entries[tage[i].tage_index].tag = tage[i].tage_tag;
          }
        }
      }
    }
  }

  if (provider_table < 11){
    if (resolveDir == TAKEN){
      tage[provider_table].entries[tage[provider_table].tage_index].counter = 
      SatIncrement(tage[provider_table].entries[tage[provider_table].tage_index].counter, 7);
    }
    else{
      tage[provider_table].entries[tage[provider_table].tage_index].counter = 
      SatDecrement(tage[provider_table].entries[tage[provider_table].tage_index].counter);
    }

    if (provider_pred == resolveDir){
      tage[provider_table].entries[tage[provider_table].tage_index].useful = 
      SatIncrement(tage[provider_table].entries[tage[provider_table].tage_index].useful, 3);
    }
    else {
      tage[provider_table].entries[tage[provider_table].tage_index].useful = 
      SatDecrement(tage[provider_table].entries[tage[provider_table].tage_index].useful);
    }

    if (((tage[provider_table].entries[tage[provider_table].tage_index].counter == 3) ||
        (tage[provider_table].entries[tage[provider_table].tage_index].counter == 4)) &&
        (tage[provider_table].entries[tage[provider_table].tage_index].useful == 0)){
      if ((alt_pred==resolveDir) && (provider_pred!=resolveDir)){
        SatIncrement(use_alt, 15);
        if (alt_table < 11){
          if (resolveDir == TAKEN) {
            tage[alt_table].entries[tage[alt_table].tage_index].useful =
            SatIncrement(tage[alt_table].entries[tage[alt_table].tage_index].useful, 3);
          }
          else{
            tage[alt_table].entries[tage[alt_table].tage_index].useful =
            SatDecrement(tage[alt_table].entries[tage[alt_table].tage_index].useful);
          }
          tage[alt_table].entries[tage[alt_table].tage_index].useful =
          SatIncrement(tage[alt_table].entries[tage[alt_table].tage_index].useful, 3);
        }
      }
      else{
        SatDecrement(use_alt);
        if(alt_table < 11){
          tage[alt_table].entries[tage[alt_table].tage_index].useful =
          SatDecrement(tage[alt_table].entries[tage[alt_table].tage_index].useful);
        }
      }
    }       
  }
  else{
    int index = (PC & ((1 << HIST_LEN) - 1));
    UINT32 phtCounter = base_predictor[index];
    // update the PHT
    if (resolveDir == TAKEN){
      base_predictor[index] = SatIncrement(phtCounter, PHT_CTR_MAX);
    }else{
      base_predictor[index] = SatDecrement(phtCounter);
    }
  }

  // update the GHR
  global_hist = (global_hist << 1);
  path_hist = (path_hist << 1);
  if(resolveDir == TAKEN){
    global_hist.set(0,1);
    path_hist++;
  }
  path_hist = path_hist % 32;

  for (int i=0; i<11; i++) {
    Compress(&tage[i].compressed_index);
    Compress(&tage[i].compressed_tag[0]);
    Compress(&tage[i].compressed_tag[1]);
  }

  clock++;
  if (clock == 256*1024) {
    clock = 0;
    if (flip){
      for (int i=0; i<11; i++){
        for (int j=0; j<1024; j++){
          tage[i].entries[j].useful = tage[i].entries[j].useful & 1;
        }
      }
      flip = false;
    }
    else{
      for (int i=0; i<11; i++){
        for (int j=0; j<1024; j++){
          tage[i].entries[j].useful = tage[i].entries[j].useful & 2;
        }
      }
      flip = true;
    }
  }

}

/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

void    PREDICTOR::TrackOtherInst(UINT32 PC, OpType opType, UINT32 branchTarget){

  // This function is called for instructions which are not
  // conditional branches, just in case someone decides to design
  // a predictor that uses information from such instructions.
  // We expect most contestants to leave this function untouched.

  return;
}

/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

void    PREDICTOR::Compress(CompressedHistory *h){
  h->compressed_history = (h->compressed_history << 1) | global_hist[0];
  h->compressed_history = h->compressed_history ^ (global_hist[h->original_length] << (h->original_length % h->compressed_length));
  h->compressed_history = h->compressed_history ^ (h->compressed_history >> h->compressed_length);
  h->compressed_history = h->compressed_history & ((1 << h->compressed_length)-1);
}