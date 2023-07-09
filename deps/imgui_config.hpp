struct ImGuiContext;
extern thread_local ImGuiContext *GImGuiThreadLocal;

#define GImGui GImGuiThreadLocal
