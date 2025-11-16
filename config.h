#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

// Max số SIG model đọc được trong DCD
#define MAX_SIG_MODELS       25

// Max số Vendor Model đọc được trong DCD
#define MAX_VENDOR_MODELS    4

// ---- DCD Struct ---------------------------------------------------

typedef struct {
  uint16_t model_id;
  uint16_t vendor_id;
} tsModel;

typedef struct {
  uint16_t SIG_models[MAX_SIG_MODELS];
  uint8_t numSIGModels;

  tsModel vendor_models[MAX_VENDOR_MODELS];
  uint8_t numVendorModels;
} tsDCD_ElemContent;

typedef struct {
  uint16_t companyID;
  uint16_t productID;
  uint16_t version;
  uint16_t replayCap;
  uint16_t featureBitmask;
  uint8_t payload[1];
} tsDCD_Header;

typedef struct {
  uint16_t location;
  uint8_t numSIGModels;
  uint8_t numVendorModels;
  uint8_t payload[1];
} tsDCD_Elem;

// ---- API ----------------------------------------------------------

extern tsDCD_ElemContent _sDCD_Prim;
extern tsDCD_ElemContent _sDCD_2nd;

extern uint8_t _dcd_raw[256];
extern uint8_t _dcd_raw_len;

void DCD_decode(void);
void DCD_decode_element(tsDCD_Elem *pElem, tsDCD_ElemContent *pDest);

#endif
