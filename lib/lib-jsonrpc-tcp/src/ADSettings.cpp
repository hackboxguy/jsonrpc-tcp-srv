#include "ADSettings.hpp"
#include "ADCommon.hpp"
#include "settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
using namespace std;
int ADSettings::identify_chain_element(void *element, int ident,
                                       ADChainProducer *pObj) {
  SETTINGS_ENTRY *pEntry;
  pEntry = (SETTINGS_ENTRY *)element;
  if ((int)pEntry->key_index == ident)
    return 0;
  return -1;
}
int ADSettings::double_identify_chain_element(void *element, int ident1,
                                              int ident2,
                                              ADChainProducer *pObj) {
  return -1;
}
void ADSettings::delete_value(ESETTINGS_KEY_TYPE key_type, void *value) {
  switch (key_type) {
  case ESETTINGS_KEY_TYPE_STRING: {
    char *myptr = (char *)value;
    ARRAY_MEM_DELETE(myptr);
  } break;
  case ESETTINGS_KEY_TYPE_INT: {
    int *myptr = (int *)value;
    ARRAY_MEM_DELETE(myptr);
  }
  case ESETTINGS_KEY_TYPE_INT_ARR: {
    int *myptr = (int *)value;
    ARRAY_MEM_DELETE(myptr);
  }
  case ESETTINGS_KEY_TYPE_LONG: {
    long *myptr = (long *)value;
    ARRAY_MEM_DELETE(myptr);
  }
  case ESETTINGS_KEY_TYPE_LONG_ARR: {
    long *myptr = (long *)value;
    ARRAY_MEM_DELETE(myptr);
  }
  case ESETTINGS_KEY_TYPE_DOUBLE: {
    double *myptr = (double *)value;
    ARRAY_MEM_DELETE(myptr);
  }
  case ESETTINGS_KEY_TYPE_DOUBLE_ARR: {
    double *myptr = (double *)value;
    ARRAY_MEM_DELETE(myptr);
  }
  default:
    break;
  }
}
int ADSettings::free_chain_element_data(void *element, ADChainProducer *pObj) {
  if (pObj->getID() == SettingsChainLocalID) {
    SETTINGS_ENTRY *objData;
    objData = (SETTINGS_ENTRY *)element;
    if (objData->value != NULL)
      delete_value(objData->key_type, objData->value);
  }
  return 0;
}
ADSettings::ADSettings() {
  SettingsState = ESETTINGS_STATE_UNKNOWN;
  filepath[0] = '\0';
  settings = NULL;
  settings_open_flag = false;
  SettingsChain.attach_helper(this);
  SettingsChain.disable_auto_remove();
  SettingsChainLocalID = SettingsChainLocal.attach_helper(this);
}
ADSettings::~ADSettings() {
  if (settings != NULL)
    settings_delete(settings);
}
int ADSettings::Is_it_valid_file(char *filepath) {
  struct stat buffer;
  if (stat(filepath, &buffer) != 0)
    return -1;
  if (buffer.st_mode & S_IFREG)
    return 0;
  return -1;
}
int ADSettings::Open() {
  if (settings_open_flag) {
    printf("settings file is already open!\n");
    return 0;
  }
  if (Is_it_valid_file(filepath) != 0) {
    return -1;
  }
  FILE *f;
  f = fopen(filepath, "r");
  if (f == NULL) {
    return -1;
  }
  settings = settings_open(f);
  fclose(f);
  if (settings == NULL) {
    return -1;
  }
  settings_open_flag = true;
  return 0;
}
int ADSettings::Save() {
  if (settings_open_flag == false) {
    return -1;
  }
  FILE *f;
  f = fopen(filepath, "w");
  if (f == NULL) {
    printf("could not open %s for writing!\n", filepath);
    return -1;
  }
  int result = settings_save(settings, f);
  fclose(f);
  if (result == 0) {
    printf("unable to save settings!\n");
    return -1;
  }
  return 0;
}
int ADSettings::get_version() { return MetaData.version; }
int ADSettings::set_version(int version) {
  MetaData.version = version;
  return 0;
}
int ADSettings::get_seq_count(int *count) { return 0; }
int ADSettings::get_size(unsigned int *size) { return 0; }
int ADSettings::get_checksum(unsigned int *cs) { return 0; }
int ADSettings::ReadStatus(char *StsMsg) {
  const char *table[] = SETTINGS_STATUS_STRING_TABLE;
  strcpy(StsMsg, table[SettingsState]);
  return 0;
}
int ADSettings::ReadMetaData(char *file, SETTINGS_META_DATA *pData) {
  pData->version = 0;
  pData->seq_count = 0;
  pData->size = 0;
  Settings *pMetaSettings = NULL;
  FILE *f;
  f = fopen(file, "r");
  if (f == NULL)
    return -1;
  pMetaSettings = settings_open(f);
  fclose(f);
  if (pMetaSettings == NULL)
    return -1;
  if (settings_get_int(pMetaSettings, SETTINGS_SECTION_META_DATA_NAME,
                       SETTINGS_SECTION_META_DATA_VERSION,
                       &pData->version) == 0)
    pData->version = -1;
  if (settings_get_int(pMetaSettings, SETTINGS_SECTION_META_DATA_NAME,
                       SETTINGS_SECTION_META_DATA_SEQ_COUNT,
                       &pData->seq_count) == 0)
    pData->seq_count = -1;
  if (settings_get_int(pMetaSettings, SETTINGS_SECTION_META_DATA_NAME,
                       SETTINGS_SECTION_META_DATA_SIZE, &pData->size) == 0)
    pData->size = -1;
  settings_delete(pMetaSettings);
  return 0;
}
int ADSettings::InitAndInsertMetaDataEntries() {
  init_setting(&MetaStngTbl[0], SETTINGS_META_ENTRY_VERSION,
               ESETTINGS_KEY_TYPE_INT, SETTINGS_SECTION_META_DATA_VERSION,
               (void *)&MetaData.version, sizeof(MetaData.version),
               ESETTINGS_SECTION_META_DATA);
  init_setting(&MetaStngTbl[1], SETTINGS_META_ENTRY_SEQCOUNT,
               ESETTINGS_KEY_TYPE_INT, SETTINGS_SECTION_META_DATA_SEQ_COUNT,
               (void *)&MetaData.seq_count, sizeof(MetaData.seq_count),
               ESETTINGS_SECTION_META_DATA);
  init_setting(&MetaStngTbl[2], SETTINGS_META_ENTRY_SIZE,
               ESETTINGS_KEY_TYPE_INT, SETTINGS_SECTION_META_DATA_SIZE,
               (void *)&MetaData.size, sizeof(MetaData.size),
               ESETTINGS_SECTION_META_DATA);
  attach_setting(&MetaStngTbl[0]);
  attach_setting(&MetaStngTbl[1]);
  attach_setting(&MetaStngTbl[2]);
  return 0;
}
int ADSettings::ValidateMetaDataEntries() {
  if (MetaStngTbl[0].key_state != ESETTINGS_KEY_STATE_LOADED)
    MetaData.version = 0;
  if (MetaStngTbl[1].key_state != ESETTINGS_KEY_STATE_LOADED)
    MetaData.seq_count = 0;
  if (MetaStngTbl[2].key_state != ESETTINGS_KEY_STATE_LOADED)
    MetaData.size = 0;
  return 0;
}
int ADSettings::UpdateSequenceCount() {
  char value[1024];
  SETTINGS_ENTRY *pEntry, *pEntryLocal;
  if (settings_open_flag == false)
    return -1;
  pEntry = (SETTINGS_ENTRY *)SettingsChain.chain_get_by_ident(
      SETTINGS_META_ENTRY_SEQCOUNT);
  if (pEntry == NULL)
    return -1;
  pEntryLocal = (SETTINGS_ENTRY *)SettingsChainLocal.chain_get_by_ident(
      SETTINGS_META_ENTRY_SEQCOUNT);
  if (pEntryLocal == NULL)
    return -1;
  MetaData.seq_count++;
  *((int *)pEntryLocal->value) = *((int *)pEntry->value);
  sprintf(value, "%d", *((int *)pEntry->value));
  settings_set(settings, pEntry->section_name, pEntry->key, value);
  return 0;
}
int ADSettings::CalculateCheckSum(char *file) {
  char cmd[1024];
  sprintf(cmd, "sha1sum %s > %s.%s", file, file, SHA1_FILE_EXTENSION);
  return system(cmd);
}
bool ADSettings::VerifyCheckSum(char *file) {
  char cmd[1024];
  sprintf(cmd, "%s.sha1", file);
  if (Is_it_valid_file(cmd) != 0)
    return false;
  sprintf(cmd, "sha1sum -c %s.%s >/dev/null 2>&1", file, SHA1_FILE_EXTENSION);
  if (system(cmd) == 0)
    return true;
  else
    return false;
}
int ADSettings::SwapAlternateFile(SETTINGS_META_DATA *pMetaData) {
  char tmpfile[1024];
  strcpy(tmpfile, pMetaData->alternate_file);
  strcpy(pMetaData->alternate_file, pMetaData->file);
  strcpy(pMetaData->file, tmpfile);
  if (pMetaData->selected_file == ESETTINGS_SELECTED_FILE_MAIN)
    pMetaData->selected_file = ESETTINGS_SELECTED_FILE_BACKUP;
  else
    pMetaData->selected_file = ESETTINGS_SELECTED_FILE_MAIN;
  return 0;
}
int ADSettings::FileConsistencyCheck(char *mainfile,
                                     SETTINGS_META_DATA *pMetaData) {
  char bkupfile[1024];
  char cmd[1100];
  if (strlen(mainfile) == 0) {
    SettingsState = ESETTINGS_STATE_NO_FILE_PASSED;
    return -1;
  }
  if (Is_it_valid_file(mainfile) != 0) {
    SettingsState = ESETTINGS_STATE_EMPTY_FILE_CREATE_ERROR;
    snprintf(cmd, sizeof(cmd), "touch %s", mainfile);
    if (system(cmd) != 0)
      return -1;
    SettingsState = ESETTINGS_STATE_EMPTY_FILE_DEFAULT_DATA;
  }
  snprintf(bkupfile, sizeof(bkupfile), "%s.%s", mainfile,
           ALTERNATE_FILE_EXTENSION);
  if (Is_it_valid_file(bkupfile) != 0) {
    snprintf(cmd, sizeof(cmd), "touch %s", bkupfile);
    if (system(cmd) != 0)
      return -1;
  }
  SETTINGS_META_DATA Metamain, Metabkup;
  ReadMetaData(mainfile, &Metamain);
  ReadMetaData(bkupfile, &Metabkup);
  bool CSMainfile = VerifyCheckSum(mainfile);
  bool CSBkupfile = VerifyCheckSum(bkupfile);
  if (CSMainfile == false && CSBkupfile == false) {
    pMetaData->selected_file = ESETTINGS_SELECTED_FILE_BACKUP;
    strcpy(pMetaData->file, bkupfile);
    strcpy(pMetaData->alternate_file, mainfile);
    if (SettingsState != ESETTINGS_STATE_EMPTY_FILE_DEFAULT_DATA)
      SettingsState = ESETTINGS_STATE_BOTH_FILES_CORRUPT;
    return 0;
  }
  if (CSMainfile == true && CSBkupfile == false) {
    pMetaData->selected_file = ESETTINGS_SELECTED_FILE_MAIN;
    strcpy(pMetaData->file, mainfile);
    strcpy(pMetaData->alternate_file, bkupfile);
    if (SettingsState != ESETTINGS_STATE_EMPTY_FILE_DEFAULT_DATA)
      SettingsState = ESETTINGS_STATE_BACKUP_FILE_CORRUPT;
    return 0;
  }
  if (CSMainfile == false && CSBkupfile == true) {
    pMetaData->selected_file = ESETTINGS_SELECTED_FILE_BACKUP;
    strcpy(pMetaData->file, bkupfile);
    strcpy(pMetaData->alternate_file, mainfile);
    if (SettingsState != ESETTINGS_STATE_EMPTY_FILE_DEFAULT_DATA)
      SettingsState = ESETTINGS_STATE_MAIN_FILE_CORRUPT;
    return 0;
  }
  if (Metamain.seq_count >= Metabkup.seq_count) {
    pMetaData->seq_count = Metamain.seq_count;
    pMetaData->version = Metamain.version;
    pMetaData->size = Metamain.size;
    strcpy(pMetaData->file, mainfile);
    strcpy(pMetaData->alternate_file, bkupfile);
    pMetaData->selected_file = ESETTINGS_SELECTED_FILE_MAIN;
    SettingsState = ESETTINGS_STATE_OK_MAIN;
    return 0;
  } else {
    pMetaData->seq_count = Metabkup.seq_count;
    pMetaData->version = Metabkup.version;
    pMetaData->size = Metabkup.size;
    strcpy(pMetaData->file, bkupfile);
    strcpy(pMetaData->alternate_file, mainfile);
    pMetaData->selected_file = ESETTINGS_SELECTED_FILE_BACKUP;
    SettingsState = ESETTINGS_STATE_OK_BACKUP;
    return 0;
  }
  return 0;
}
int ADSettings::Load(char *file) {
  filepath[0] = '\0';
  MetaData.file[0] = '\0';
  MetaData.alternate_file[0] = '\0';
  if (FileConsistencyCheck(file, &MetaData) != 0)
    return -1;
  strcpy(filepath, MetaData.file);
  InitAndInsertMetaDataEntries();
  if (Open() != 0)
    return -1;
  int len = SettingsChain.get_chain_size();
  SETTINGS_ENTRY *pEntry, *pEntryLocal;
  int result = 0;
  for (int i = 0; i < len; i++) {
    pEntry = (SETTINGS_ENTRY *)SettingsChain.chain_get_by_index(i);
    if (pEntry == NULL)
      return -1;
    pEntryLocal = (SETTINGS_ENTRY *)SettingsChainLocal.chain_get_by_index(i);
    if (pEntryLocal == NULL)
      return -1;
    result = 0;
    switch (pEntry->key_type) {
    case ESETTINGS_KEY_TYPE_STRING:
      result = settings_get(settings, pEntry->section_name, pEntry->key,
                            (char *)pEntry->value, pEntry->value_size);
      result &=
          settings_get(settings, pEntryLocal->section_name, pEntryLocal->key,
                       (char *)pEntryLocal->value, pEntryLocal->value_size);
      break;
    case ESETTINGS_KEY_TYPE_INT:
      result = settings_get_int(settings, pEntry->section_name, pEntry->key,
                                (int *)pEntry->value);
      result &= settings_get_int(settings, pEntryLocal->section_name,
                                 pEntryLocal->key, (int *)pEntryLocal->value);
      break;
    case ESETTINGS_KEY_TYPE_INT_ARR:
      result = settings_get_int_tuple(settings, pEntry->section_name,
                                      pEntry->key, (int *)pEntry->value,
                                      (pEntry->value_size) / sizeof(int));
      result &= settings_get_int_tuple(
          settings, pEntryLocal->section_name, pEntryLocal->key,
          (int *)pEntryLocal->value, (pEntryLocal->value_size) / sizeof(int));
      break;
    case ESETTINGS_KEY_TYPE_LONG:
      result = settings_get_long(settings, pEntry->section_name, pEntry->key,
                                 (long *)pEntry->value);
      result &= settings_get_long(settings, pEntryLocal->section_name,
                                  pEntryLocal->key, (long *)pEntryLocal->value);
      break;
    case ESETTINGS_KEY_TYPE_LONG_ARR:
      result = settings_get_long_tuple(settings, pEntry->section_name,
                                       pEntry->key, (long *)pEntry->value,
                                       (pEntry->value_size) / sizeof(long));
      result &= settings_get_long_tuple(
          settings, pEntryLocal->section_name, pEntryLocal->key,
          (long *)pEntryLocal->value, (pEntryLocal->value_size) / sizeof(long));
      break;
    case ESETTINGS_KEY_TYPE_DOUBLE:
      result = settings_get_double(settings, pEntry->section_name, pEntry->key,
                                   (double *)pEntry->value);
      result &=
          settings_get_double(settings, pEntryLocal->section_name,
                              pEntryLocal->key, (double *)pEntryLocal->value);
      break;
    case ESETTINGS_KEY_TYPE_DOUBLE_ARR:
      result = settings_get_double_tuple(settings, pEntry->section_name,
                                         pEntry->key, (double *)pEntry->value,
                                         (pEntry->value_size) / sizeof(double));
      result &= settings_get_double_tuple(
          settings, pEntryLocal->section_name, pEntryLocal->key,
          (double *)pEntryLocal->value,
          (pEntryLocal->value_size) / sizeof(double));
      break;
    default:
      break;
    }
    if (result == 0) {
      pEntry->key_state = ESETTINGS_KEY_STATE_NOT_FOUND;
      pEntryLocal->key_state = ESETTINGS_KEY_STATE_NOT_FOUND;
    } else {
      pEntry->key_state = ESETTINGS_KEY_STATE_LOADED;
      pEntryLocal->key_state = ESETTINGS_KEY_STATE_LOADED;
    }
  }
  ValidateMetaDataEntries();
  return 0;
}
int ADSettings::Store() {
  if (settings_open_flag == false) {
    return -1;
  }
  if (strlen(filepath) == 0) {
    if (Load(filepath) != 0)
      return -1;
  }
  char value[4096];
  char temp_arr[512];
  int len = SettingsChain.get_chain_size();
  SETTINGS_ENTRY *pEntry, *pEntryLocal;
  bool SettingsChanged = false;
  for (int i = 0; i < len; i++) {
    pEntry = (SETTINGS_ENTRY *)SettingsChain.chain_get_by_index(i);
    if (pEntry == NULL)
      return -1;
    pEntryLocal = (SETTINGS_ENTRY *)SettingsChainLocal.chain_get_by_index(i);
    if (pEntryLocal == NULL)
      return -1;
    switch (pEntry->key_type) {
    case ESETTINGS_KEY_TYPE_STRING:
      sprintf(value, "%s", (char *)pEntry->value);
      if (strcmp((char *)pEntry->value, (char *)pEntryLocal->value) != 0) {
        SettingsChanged = true;
        strcpy((char *)pEntryLocal->value, (char *)pEntry->value);
      }
      break;
    case ESETTINGS_KEY_TYPE_INT:
      sprintf(value, "%d", *((int *)pEntry->value));
      if (*((int *)pEntry->value) != *((int *)pEntryLocal->value)) {
        SettingsChanged = true;
        *((int *)pEntryLocal->value) = *((int *)pEntry->value);
      }
      break;
    case ESETTINGS_KEY_TYPE_INT_ARR: {
      int *val = ((int *)pEntry->value);
      int *localval = ((int *)pEntryLocal->value);
      int len = pEntry->value_size / sizeof(int);
      value[0] = '\0';
      for (int i = 0; i < len; i++) {
        if (i == (len - 1))
          sprintf(temp_arr, "%d", *val);
        else
          sprintf(temp_arr, "%d, ", *val);
        strcat(value, temp_arr);
        if (*val != *localval) {
          SettingsChanged = true;
          *localval = *val;
        }
        val++;
        localval++;
      }
    } break;
    case ESETTINGS_KEY_TYPE_LONG:
      sprintf(value, "%ld", *((long *)pEntry->value));
      if (*((long *)pEntry->value) != *((long *)pEntryLocal->value)) {
        SettingsChanged = true;
        *((long *)pEntryLocal->value) = *((long *)pEntry->value);
      }
      break;
    case ESETTINGS_KEY_TYPE_LONG_ARR: {
      long *val = ((long *)pEntry->value);
      long *localval = ((long *)pEntryLocal->value);
      int len = pEntry->value_size / sizeof(long);
      value[0] = '\0';
      for (int i = 0; i < len; i++) {
        if (i == (len - 1))
          sprintf(temp_arr, "%ld", *val);
        else
          sprintf(temp_arr, "%ld, ", *val);
        strcat(value, temp_arr);
        if (*val != *localval) {
          SettingsChanged = true;
          *localval = *val;
        }
        val++;
        localval++;
      }
    } break;
    case ESETTINGS_KEY_TYPE_DOUBLE:
      sprintf(value, "%E", *((double *)pEntry->value));
      if (*((double *)pEntry->value) != *((double *)pEntryLocal->value)) {
        SettingsChanged = true;
        *((double *)pEntryLocal->value) = *((double *)pEntry->value);
      }
      break;
    case ESETTINGS_KEY_TYPE_DOUBLE_ARR: {
      double *val = ((double *)pEntry->value);
      double *localval = ((double *)pEntryLocal->value);
      int len = pEntry->value_size / sizeof(double);
      value[0] = '\0';
      for (int i = 0; i < len; i++) {
        if (i == (len - 1))
          sprintf(temp_arr, "%E", *val);
        else
          sprintf(temp_arr, "%E, ", *val);
        strcat(value, temp_arr);
        if (*val != *localval) {
          SettingsChanged = true;
          *localval = *val;
        }
        val++;
        localval++;
      }
      SettingsChanged = true;
    } break;
    default:
      sprintf(value, "unknown");
      break;
    }
    settings_set(settings, pEntry->section_name, pEntry->key, value);
  }
  if (SettingsChanged == true) {
    strcpy(filepath, MetaData.alternate_file);
    UpdateSequenceCount();
    int result = Save();
    if (result != 0)
      return result;
    SwapAlternateFile(&MetaData);
    return CalculateCheckSum(filepath);
  } else {
    return 0;
  }
}
int ADSettings::init_setting(SETTINGS_ENTRY *pEntry, unsigned int indx,
                             ESETTINGS_KEY_TYPE type, const char *name,
                             void *val, size_t val_size,
                             ESETTINGS_SECTION section) {
  pEntry->key_index = indx;
  pEntry->key_type = type;
  strcpy(pEntry->key, name);
  pEntry->value = val;
  pEntry->value_size = val_size;
  pEntry->key_state = ESETTINGS_KEY_STATE_NOT_LOADED;
  pEntry->section = section;
  if (pEntry->section == ESETTINGS_SECTION_META_DATA)
    strcpy(pEntry->section_name, SETTINGS_SECTION_META_DATA_NAME);
  else
    strcpy(pEntry->section_name, SETTINGS_SECTION_DATA_NAME);
  return 0;
}
int ADSettings::attach_setting(SETTINGS_ENTRY *pEntry) {
  SETTINGS_ENTRY *pNewEntry = NULL;
  OBJECT_MEM_NEW(pNewEntry, SETTINGS_ENTRY);
  if (pNewEntry == NULL)
    return -1;
  pNewEntry->key_index = pEntry->key_index;
  pNewEntry->key_type = pEntry->key_type;
  strcpy(pNewEntry->key, pEntry->key);
  pNewEntry->value = NULL;
  ARRAY_MEM_NEW(pNewEntry->value, pEntry->value_size);
  pNewEntry->value_size = pEntry->value_size;
  pNewEntry->key_state = pEntry->key_state;
  pNewEntry->section = pEntry->section;
  strcpy(pNewEntry->section_name, pEntry->section_name);
  if (SettingsChainLocal.chain_put((void *)pNewEntry) != 0) {
    delete_value(pNewEntry->key_type, pNewEntry->value);
    OBJ_MEM_DELETE(pNewEntry);
    return -1;
  }
  if (SettingsChain.chain_put((void *)pEntry) != 0)
    return -1;
  return 0;
}
