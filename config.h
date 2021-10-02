

#define CONFIG_MAX_LINE    1024
typedef struct configEnum {
    const char *name;
    const int val;
} configEnum;
#define ETL_CRC32 1
configEnum etl_type_enum[] = {
    {"crc32", ETL_CRC32},
    {NULL, 0}
};

void loadServerConfigFromString(char *config);
void initEtlServerConfig(void);
void resetServerSaveParams();

