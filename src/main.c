#include <esp_log.h>
#include <coap3/coap.h>

static const char *TAG = "CoAP_server";
static int contador = 0;
static coap_context_t *coap_ctx = NULL;

// Função de tratamento para o recurso "contador"
static void handle_get_contador(coap_context_t *ctx, struct coap_resource_t *resource,
                                const coap_endpoint_t *local_interface, coap_address_t *peer,
                                coap_pdu_t *request, coap_pdu_t *response) {
    ESP_LOGI(TAG, "Received a GET request for contador");
    char payload[16];
    snprintf(payload, sizeof(payload), "%d", contador);

    // Definir o código de resposta (2.05 Content)
    coap_pdu_set_code(response, COAP_RESPONSE_CODE_CONTENT);

    // Definir o payload da resposta
    coap_add_data(response, strlen(payload), (const uint8_t *)payload);

    // Enviar a resposta
    coap_send(ctx, response);
}

// Função para notificar observadores com dados personalizados
static void notify_observers() {
    if (!coap_ctx) {
        ESP_LOGE(TAG, "CoAP context is not initialized");
        return;
    }

    // Criar uma resposta de notificação personalizada
    coap_pdu_t *notification = coap_pdu_init(0, 0, 0, 200);
    if (!notification) {
        ESP_LOGE(TAG, "Failed to create CoAP notification PDU");
        return;
    }

    // Definir o código de resposta (2.05 Content)
    coap_pdu_set_code(notification, COAP_RESPONSE_CODE_CONTENT);

    // Definir o payload da notificação
    const char *data = "aumento identificado";
    coap_add_data(notification, strlen(data), (const uint8_t *)data);

    // Notificar observadores registrados para o recurso "contador"
    coap_resource_notify_observers(notification, "/contador");

    // Liberar recursos
    coap_delete_pdu(notification);
}

void coap_server_task(void *pvParameters) {
    coap_address_t addr;
    coap_endpoint_t *endpoint;

    coap_address_init(&addr);
    addr.addr.sin.sin_family = AF_INET;
    addr.addr.sin.sin_port = htons(COAP_DEFAULT_PORT);
    addr.addr.sin.sin_addr.s_addr = htonl(INADDR_ANY);

    coap_ctx = coap_new_context(&addr);
    if (!coap_ctx) {
        ESP_LOGE(TAG, "Failed to create CoAP context");
        vTaskDelete(NULL);
    }

    endpoint = coap_new_endpoint(coap_ctx, &addr, COAP_PROTO_UDP);
    if (!endpoint) {
        ESP_LOGE(TAG, "Failed to create CoAP endpoint");
        coap_free_context(coap_ctx);
        vTaskDelete(NULL);
    }

    // Registro do recurso "contador" no servidor
    coap_resource_t *contador_resource = coap_resource_init((unsigned char *)"contador", 0);
    coap_register_handler(contador_resource, COAP_REQUEST_GET, handle_get_contador);
    coap_add_resource(coap_ctx, contador_resource);

    while (1) {
        contador++;
        ESP_LOGI(TAG, "Incrementing contador: %d", contador);
        notify_observers();
        vTaskDelay(pdMS_TO_TICKS(5000));
    }

    coap_delete_resource(coap_ctx, contador_resource);
    coap_free_endpoint(endpoint);
    coap_free_context(coap_ctx);
    coap_cleanup();
    coap_ctx = NULL;
    vTaskDelete(NULL);
}

void app_main() {
    // Inicialização do ESP-IDF e outras configurações necessárias

    // Crie uma tarefa para o servidor CoAP
    xTaskCreate(&coap_server_task, "coap_server_task", 4096, NULL, 5, NULL);
}
