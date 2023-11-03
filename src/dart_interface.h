#ifndef ORG_FIRO_SPARK_DART_INTERFACE_H
#define ORG_FIRO_SPARK_DART_INTERFACE_H

#include "../include/spark.h"

const char *generateSpendKey();

const char *createSpendKey(const char * r);

const char *createFullViewKey(const char * spend_key_r);

#endif //ORG_FIRO_SPARK_DART_INTERFACE_H
