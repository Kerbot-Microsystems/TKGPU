#ifndef TKGPU_FILE_HANDLERS_H
#define TKGPU_FILE_HANDLERS_H
#include "../TKGPUNotifier.H"
#ifndef _WIN32
int tg_file_dispatch(tg_notifier *s);
int tg_file_pending(const tg_notifier *s);
#endif
#endif
