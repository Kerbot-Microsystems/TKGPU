#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((visibility("default")))
#endif
EXPORT int Fixture_Init(void *interp) { (void)interp; return 0; }
EXPORT int Fixture_SafeInit(void *interp) { (void)interp; return 0; }
EXPORT int Fixture_Answer(void) { return 42; }
