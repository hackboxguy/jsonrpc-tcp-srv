#ifndef __AD_SETTINGS_H_
#define __AD_SETTINGS_H_
#include "settings.h"
#define SETTINGS_SECTION_DATA_NAME "Data"
#define SETTINGS_SECTION_META_DATA_NAME "MetaData"
#define SETTINGS_SECTION_META_DATA_VERSION "Version"
#define SETTINGS_SECTION_META_DATA_SEQ_COUNT "SequenceCount"
#define SETTINGS_SECTION_META_DATA_SIZE "Size"
#define SETTINGS_SECTION_META_DATA_CHECKSUM "Checksum"
#define SHA1_FILE_EXTENSION "sha1"
#define ALTERNATE_FILE_EXTENSION "alt"
#include "ADGenericChain.hpp"
#define SETTINGS_STATUS_STRING_TABLE                                           \
  {                                                                            \
    "OkMain", "OkBkup", "OkFmwOvride", "NoFile", "EmptyFileErr",               \
        "EmptyFileDef", "MainCorrupt", "BkupCorrupt", "BothCorrupt",           \
        "unknown", "none", "\0"                                                \
  }
typedef enum ESETTINGS_STATE_T {
  ESETTINGS_STATE_OK_MAIN,
  ESETTINGS_STATE_OK_BACKUP,
  ESETTINGS_STATE_OK_OVERRIDE_DATA,
  ESETTINGS_STATE_NO_FILE_PASSED,
  ESETTINGS_STATE_EMPTY_FILE_CREATE_ERROR,
  ESETTINGS_STATE_EMPTY_FILE_DEFAULT_DATA,
  ESETTINGS_STATE_MAIN_FILE_CORRUPT,
  ESETTINGS_STATE_BACKUP_FILE_CORRUPT,
  ESETTINGS_STATE_BOTH_FILES_CORRUPT,
  ESETTINGS_STATE_UNKNOWN,
  ESETTINGS_STATE_NONE
} ESETTINGS_STATE;
#define SETTINGS_NAME_MAX_LENGTH 1024
typedef enum ESETTINGS_KEY_TYPE_T {
  ESETTINGS_KEY_TYPE_STRING,
  ESETTINGS_KEY_TYPE_INT,
  ESETTINGS_KEY_TYPE_INT_ARR,
  ESETTINGS_KEY_TYPE_LONG,
  ESETTINGS_KEY_TYPE_LONG_ARR,
  ESETTINGS_KEY_TYPE_DOUBLE,
  ESETTINGS_KEY_TYPE_DOUBLE_ARR,
  ESETTINGS_KEY_TYPE_UNKNOWN,
  ESETTINGS_KEY_TYPE_NONE
} ESETTINGS_KEY_TYPE;
typedef enum ESETTINGS_KEY_STATE_T {
  ESETTINGS_KEY_STATE_NOT_LOADED,
  ESETTINGS_KEY_STATE_LOADED,
  ESETTINGS_KEY_STATE_NOT_FOUND,
  ESETTINGS_KEY_STATE_STORED,
  ESETTINGS_KEY_STATE_UNKNOWN,
  ESETTINGS_KEY_STATE_NONE
} ESETTINGS_KEY_STATE;
typedef enum ESETTINGS_SELECTED_FILE_T {
  ESETTINGS_SELECTED_FILE_MAIN,
  ESETTINGS_SELECTED_FILE_BACKUP,
  ESETTINGS_SELECTED_FILE_UNKNOWN,
  ESETTINGS_SELECTED_FILE_NONE
} ESETTINGS_SELECTED_FILE;
typedef enum ESETTINGS_SECTION_T {
  ESETTINGS_SECTION_DATA,
  ESETTINGS_SECTION_META_DATA,
  ESETTINGS_SECTION_UNKNOWN,
  ESETTINGS_SECTION_NONE
} ESETTINGS_SECTION;
typedef struct SETTINGS_ENTRY_T {
  unsigned int key_index;
  ESETTINGS_KEY_TYPE key_type;
  char key[SETTINGS_NAME_MAX_LENGTH];
  void *value;
  size_t value_size;
  ESETTINGS_KEY_STATE key_state;
  ESETTINGS_SECTION section;
  char section_name[SETTINGS_NAME_MAX_LENGTH];
} SETTINGS_ENTRY;
typedef enum SETTINGS_META_ENTRY_T {
  SETTINGS_META_ENTRY_VERSION = 65000,
  SETTINGS_META_ENTRY_SEQCOUNT = 65001,
  SETTINGS_META_ENTRY_SIZE = 65002,
  SETTINGS_META_ENTRY_SHA1HASH = 65003,
  SETTINGS_META_ENTRY_END = 65004,
  SETTINGS_META_ENTRY_UNKNOWN = 65005,
  SETTINGS_META_ENTRY_NONE = 65006
} SETTINGS_META_ENTRY;
typedef struct SETTINGS_META_DATA_T {
  int version;
  int seq_count;
  int size;
  char sha1hash[50];
  char file[1024];
  char alternate_file[1024];
  ESETTINGS_SELECTED_FILE selected_file;
} SETTINGS_META_DATA;
class ADSettings : public ADChainConsumer {
  SETTINGS_META_DATA MetaData;
  SETTINGS_ENTRY MetaStngTbl[5];
  ESETTINGS_STATE SettingsState;
  int SettingsChainLocalID;
  ADGenericChain SettingsChain, SettingsChainLocal;
  char filepath[1024];
  Settings *settings;
  bool settings_open_flag;
  virtual int identify_chain_element(void *element, int ident,
                                     ADChainProducer *pObj);
  virtual int double_identify_chain_element(void *element, int ident1,
                                            int ident2, ADChainProducer *pObj);
  virtual int free_chain_element_data(void *element, ADChainProducer *pObj);
  int Open();
  int Save();
  int FileConsistencyCheck(char *mainfile, SETTINGS_META_DATA *pMetaData);
  int InitAndInsertMetaDataEntries();
  int ValidateMetaDataEntries();
  int UpdateSequenceCount();
  int CalculateCheckSum(char *file);
  bool VerifyCheckSum(char *file);
  int ReadMetaData(char *file, SETTINGS_META_DATA *pData);
  int SwapAlternateFile(SETTINGS_META_DATA *pData);
  void delete_value(ESETTINGS_KEY_TYPE key_type, void *value);

public:
  ADSettings();
  ~ADSettings();
  int Load(char *file);
  int Store();
  int get_version();
  int set_version(int version);
  int get_seq_count(int *count);
  int get_size(unsigned int *size);
  int get_checksum(unsigned int *cs);
  int attach_setting(SETTINGS_ENTRY *pEntry);
  int init_setting(SETTINGS_ENTRY *pEntry, unsigned int indx,
                   ESETTINGS_KEY_TYPE type, const char *name, void *val,
                   size_t val_size,
                   ESETTINGS_SECTION section = ESETTINGS_SECTION_DATA);
  int Is_it_valid_file(char *filepath);
  int ReadStatus(char *StsMsg);
};
#endif
