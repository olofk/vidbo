#include "vidbo.h"

/* I hate myself for implementing a linked list here. Is this really the way to do it? */
struct Node {
  unsigned long time;
  char *group;
  char *item;
  int value;
  struct Node* next;
};

struct Node *head = NULL;

struct msg {
  void *payload; /* is malloc'd */
  size_t len;
};

struct per_session_data__minimal {
  struct per_session_data__minimal *pss_list;
  struct lws *wsi;
};

/* one of these is created for each vhost our protocol is used with */

struct per_vhost_data__minimal {
  struct lws_context *context;
  struct lws_vhost *vhost;
  const struct lws_protocols *protocol;
  struct per_session_data__minimal *pss_list; /* linked-list of live pss*/
  struct msg amsg; /* the one pending message... */
};

int connected = 0;
int pending = 0;
int cts = 1;
char json_buf[1024];
const char * const json_template = "{\"%s\" : {\"%s\" : %d}}";
int *input_vals;

static void __minimal_destroy_message(void *_msg) {
  struct msg *msg = (struct msg *)_msg;
  free(msg->payload);
  msg->payload = NULL;
  msg->len = 0;
}

static void append(struct Node** head_ref, unsigned long _time, char const *group, char const *item, int val) {
  struct Node* new_node = (struct Node*) malloc(sizeof(struct Node));

  struct Node *last = *head_ref;

  new_node->time = _time;
  new_node->group = strdup(group);
  new_node->item = strdup(item);
  new_node->value = val;
  new_node->next = NULL;
 
  if (*head_ref == NULL) {
    *head_ref = new_node;
  } else {
    while (last->next != NULL)
      last = last->next;
    last->next = new_node;
  }
}

void deleteHead(struct Node ** head_ref) {
  struct Node *temp = *head_ref;
  if (temp) {
    *head_ref = temp->next; // Changed head
    free(temp->group);
    free(temp->item);
    free(temp); // free old head
  }  
}
void printList(struct Node *node) {
  while (node != NULL)
  {
    printf(" %lu %s %s %d\n", node->time, node->group, node->item, node->value);
     node = node->next;
  }
}

static signed char lejp_cb(struct lejp_ctx *ctx, char reason) {
  if (reason & LEJP_FLAG_CB_IS_VALUE) {
    if (ctx->path_match) {
      input_vals[ctx->path_match-1] = atoi(ctx->buf);
    }
    else
      printf("No match for %s\n", ctx->path);
  }
  return 0;
}


void *input_paths;
int input_count;

void vidbo_register_inputs(void * inputs, int count) {
  input_vals = (int *)calloc(count, sizeof(int));
  input_paths = inputs;
  input_count = count;
}

static void parse_json(uint8_t *buf, int len) {
  struct lejp_ctx lejp_ctx;
  lejp_construct(&lejp_ctx, lejp_cb, NULL, (const char * const *)input_paths, input_count);
  int m = lejp_parse(&lejp_ctx, (uint8_t *)buf, 1024);
  if (m < 0 && m != LEJP_CONTINUE) {
    lwsl_err("parse failed %d\n", m);
  }
  lejp_destruct(&lejp_ctx);
}


static int ws_cb(struct lws *wsi, enum lws_callback_reasons reason,
		 void *user, void *in, size_t len)
{
  struct per_session_data__minimal *pss = (struct per_session_data__minimal *)user;
  struct per_vhost_data__minimal   *vhd = (struct per_vhost_data__minimal *)
    lws_protocol_vh_priv_get(lws_get_vhost(wsi),
			     lws_get_protocol(wsi));

  int m;
  unsigned long cur_time;
  struct Node *temp = NULL, *next;
  struct Node *gpio_head = NULL;
  struct Node *gpio_cur = NULL;
  struct Node *serial_head = NULL;
  struct Node *serial_cur = NULL;

  int chars_written = 0;
  switch (reason) {
  case LWS_CALLBACK_PROTOCOL_INIT:
    vhd = (struct per_vhost_data__minimal *)lws_protocol_vh_priv_zalloc(lws_get_vhost(wsi),
				      lws_get_protocol(wsi),
				      sizeof(struct per_vhost_data__minimal));
    vhd->context = lws_get_context(wsi);
    vhd->protocol = lws_get_protocol(wsi);
    vhd->vhost = lws_get_vhost(wsi);
    break;

  case LWS_CALLBACK_ESTABLISHED:
    /* add ourselves to the list of live pss held in the vhd */
    lws_ll_fwd_insert(pss, pss_list, vhd->pss_list);
    pss->wsi = wsi;
    connected = 1;
    break;

  case LWS_CALLBACK_CLOSED:
    /* remove our closing pss from the list of live pss */
    lws_ll_fwd_remove(struct per_session_data__minimal, pss_list,
		      pss, vhd->pss_list);
    connected = 0;
    break;

  case LWS_CALLBACK_SERVER_WRITEABLE:

    temp = head;

    if (!temp)
      break;

    cur_time = temp->time;

    /*Split list of pending updates into a gpio and a serial list */
    while (head && (head->time == cur_time)) {

      if (!strcmp(head->group, "gpio")) {
	append(&gpio_head, head->time, head->group, head->item, head->value);
      } else if (!strcmp(head->group, "serial")) {
	append(&serial_head, head->time, head->group, head->item, head->value);
      } else {
	printf("Unknown type %s. Must be serial or gpio\n", head->group);
      }
      deleteHead(&head);
    }

    /* Print time */
    chars_written += sprintf(json_buf, "{\"time\" : %ld", cur_time);

    if (gpio_head) {
      chars_written += sprintf(json_buf+chars_written, ", \"gpio\" : {");
      while (gpio_head) {
	chars_written += sprintf(json_buf+chars_written, "\"%s\" : %d, ", gpio_head->item, gpio_head->value);
	deleteHead(&gpio_head);
      }

      /* Hack. Overwrite final comma */
      chars_written += sprintf(json_buf+chars_written-2, "} ") - 2;
    }
    
    if (serial_head) {
      chars_written += sprintf(json_buf+chars_written, ", \"serial\" : {");
      while (serial_head) {
	chars_written += sprintf(json_buf+chars_written, "\"%s\" : %d, ", serial_head->item, serial_head->value);
	deleteHead(&serial_head);
      }
      chars_written += sprintf(json_buf+chars_written-2, "} ") - 2;
    }

    /* Close JSON */
    chars_written += sprintf(json_buf+chars_written, "}");
    //printf("%s\n", json_buf);
    if (vhd->amsg.payload)
      __minimal_destroy_message(&vhd->amsg);

    vhd->amsg.len = strlen(json_buf);

    /* notice we over-allocate by LWS_PRE */
    vhd->amsg.payload = malloc(LWS_PRE + vhd->amsg.len);
    memcpy((uint8_t *)vhd->amsg.payload+LWS_PRE, json_buf, vhd->amsg.len);

    if (!vhd->amsg.payload) {
      break;
    }

    /* notice we allowed for LWS_PRE in the payload already */
    m = lws_write(wsi, ((unsigned char *)vhd->amsg.payload) +
		  LWS_PRE, vhd->amsg.len, LWS_WRITE_TEXT);
    if (m < (int)vhd->amsg.len) {
      lwsl_err("ERROR %d writing to ws\n", m);
      return -1;
    }
    cts = 1;

    break;

  case LWS_CALLBACK_RECEIVE:
    parse_json((uint8_t *)in, len);
    pending = 1;

    /*
     * let everybody know we want to write something on them
     * as soon as they are ready
     */
    lws_start_foreach_llp(struct per_session_data__minimal **,
			  ppss, vhd->pss_list) {
      lws_callback_on_writable((*ppss)->wsi);
    } lws_end_foreach_llp(ppss, pss_list);
    break;

  default:
    break;
  }

  return 0;
}

struct lws_protocols protocols[] = {
  {"ws", ws_cb, sizeof(struct per_session_data__minimal), 1024, 0, NULL, 0},
  { NULL, NULL, 0, 0 } /* terminator */
};

int vidbo_init(vidbo_context_t *context, uint16_t port) {
  struct lws_context *lws_ctx;
  struct lws_context_creation_info info;

  memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */
  info.port = port;
  info.protocols = protocols;
  info.vhost_name = "localhost";
  info.options =
    LWS_SERVER_OPTION_HTTP_HEADERS_SECURITY_BEST_PRACTICES_ENFORCE;
  lws_ctx = lws_create_context(&info);
  if (!lws_ctx) {
    lwsl_err("lws init failed\n");
    return 0;
  }
  int logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE;
  lws_set_log_level(logs, NULL);
  context->context = lws_ctx;
  context->protocols = protocols;
  return 0;
}

int vidbo_send(vidbo_context_t *context, unsigned long _time, char const *group, char const *item, int val) {


  append(&head, _time, group, item, val);
  lws_callback_on_writable_all_protocol(context->context,
					context->protocols);
  return 1;
}

//Return 1 if new message was received
int vidbo_recv(vidbo_context_t *context, int *inputs) {
  int retval = 0;
  if (pending) {
    memcpy(inputs, input_vals, input_count*sizeof(int));
    pending = 0;
    retval = 1;
  }
  lws_callback_on_writable_all_protocol(context->context,
					context->protocols);
  lws_service_tsi(context->context, -1, 0);
  return retval;
}

void vidbo_destroy(vidbo_context_t *context) {
  lws_context_destroy(context->context);
}
