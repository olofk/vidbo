#include <libwebsockets.h>

typedef const char * const vidbo_input;

typedef struct {
  struct lws_context *context;
  struct lws_protocols *protocols;
} vidbo_context_t;

int  vidbo_init(vidbo_context_t * context, uint16_t port);
int  vidbo_send(vidbo_context_t *context, unsigned long time, char const *group, char const *item, int val);
int  vidbo_recv(vidbo_context_t *context, int *inputs);
void vidbo_register_inputs(vidbo_context_t *context, vidbo_input * inputs, size_t count);
void vidbo_destroy(vidbo_context_t *context);


