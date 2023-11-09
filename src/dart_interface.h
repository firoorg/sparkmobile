#ifndef ORG_FIRO_SPARK_DART_INTERFACE_H
#define ORG_FIRO_SPARK_DART_INTERFACE_H

extern "C" {

const char* getAddress(const char* keyData, int index, int diversifier);

/*
const char *createFullViewKey(const char* keyData, int index);

const char* createIncomingViewKey(const char* keyData, int index);
*/

} // extern "C"

#endif //ORG_FIRO_SPARK_DART_INTERFACE_H
