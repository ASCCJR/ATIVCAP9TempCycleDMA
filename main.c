/**
 * ------------------------------------------------------------
 * Arquivo: main.c
 * Projeto: TempCycleDMA
 * ------------------------------------------------------------
 * Descrição:
 * Ciclo principal do sistema embarcado, baseado em um
 * executor cíclico com 4 tarefas principais, cada uma
 * controlada por seu próprio timer periódico:
 *
 * Tarefa 1 - Leitura da temperatura via DMA (meio segundo)
 * Tarefa 2 - Exibição da temperatura e tendência no OLED
 * Tarefa 3 - Análise da tendência da temperatura
 * Tarefa 4 - Controle da matriz NeoPixel baseada na tendência
 *
 * As tarefas são sincronizadas por flags globais.
 * O sistema utiliza watchdog para segurança (opcional) e
 * terminal USB para monitoramento.
 *
 * Data: 12/05/2025
 * ------------------------------------------------------------
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/watchdog.h"
#include "hardware/timer.h"
#include "pico/stdio_usb.h" // Para printf

#include "setup.h"
#include "tarefa1_temp.h"
#include "tarefa2_display.h"
#include "tarefa3_tendencia.h"
#include "tarefa4_controla_neopixel.h"

// --- Variáveis Globais para Dados e Sincronização ---
float g_media_temperatura;
tendencia_t g_tendencia_atual;

volatile bool g_flag_media_calculada = false;     // Sinaliza que T1 concluiu, média pronta para T3
volatile bool g_flag_tendencia_analisada = false; // Sinaliza que T3 concluiu, tendência pronta para T2, T4 e print
volatile bool g_flag_permitir_print = false;    // Sinaliza para o loop principal que pode imprimir dados novos

// --- Configurações de DMA  ---
extern dma_channel_config cfg_temp;
extern int DMA_TEMP_CHANNEL;

// --- Objetos Timer ---
static repeating_timer_t timer_t1;
static repeating_timer_t timer_t2;
static repeating_timer_t timer_t3;
static repeating_timer_t timer_t4;

// --- Protótipos dos Callbacks dos Timers ---
bool timer_callback_tarefa1(repeating_timer_t *rt);
bool timer_callback_tarefa2(repeating_timer_t *rt);
bool timer_callback_tarefa3(repeating_timer_t *rt);
bool timer_callback_tarefa4(repeating_timer_t *rt);

// --- Timer callback para Tarefa 1: Leitura de Temperatura ---
bool timer_callback_tarefa1(repeating_timer_t *rt) {
    // Invalida flags de dados processados do ciclo anterior
    g_flag_tendencia_analisada = false;
    g_flag_permitir_print = false;

    // Realiza a leitura da temperatura
    g_media_temperatura = tarefa1_obter_media_temp(&cfg_temp, DMA_TEMP_CHANNEL);
    
    // Sinaliza que a nova média está pronta para a Tarefa 3
    g_flag_media_calculada = true;
    
    return true; // Continua repetindo o timer
}

// --- Timer callback para Tarefa 3: Análise de Tendência ---
bool timer_callback_tarefa3(repeating_timer_t *rt) {
    if (g_flag_media_calculada) {
        g_tendencia_atual = tarefa3_analisa_tendencia(g_media_temperatura);
        
        g_flag_media_calculada = false; // Consome a flag da média
        g_flag_tendencia_analisada = true; // Sinaliza que a tendência está pronta
        g_flag_permitir_print = true;      // Sinaliza para o loop principal imprimir
    }
    return true; 
}

// --- Timer callback para Tarefa 2: Exibição no OLED ---
bool timer_callback_tarefa2(repeating_timer_t *rt) {
    if (g_flag_tendencia_analisada) {
        // g_media_temperatura e g_tendencia_atual são válidos aqui
        tarefa2_exibir_oled(g_media_temperatura, g_tendencia_atual);
    }
    return true; 
}

// --- Timer callback para Tarefa 4: Controle NeoPixel ---
bool timer_callback_tarefa4(repeating_timer_t *rt) {
    if (g_flag_tendencia_analisada) {
        // g_tendencia_atual é válida aqui
        tarefa4_matriz_cor_por_tendencia(g_tendencia_atual);
    }
    return true; 
}


int main() {
    setup(); // Configurações iniciais de hardware, USB, DMA, etc.

    // Ativa o watchdog com timeout de 2 segundos (opcional, descomentar se necessário)
    // watchdog_enable(2000, 1);

    // Configura e adiciona os timers para cada tarefa
    // Todas as tarefas rodarão com um ciclo base de 1 segundo.
    // A Tarefa 1 (leitura de temp) leva 0.5s para ser executada.
    // As demais são mais rápidas e dependem das flags.
    add_repeating_timer_ms(1000, timer_callback_tarefa1, NULL, &timer_t1);
    add_repeating_timer_ms(1000, timer_callback_tarefa3, NULL, &timer_t3);
    add_repeating_timer_ms(1000, timer_callback_tarefa2, NULL, &timer_t2);
    add_repeating_timer_ms(1000, timer_callback_tarefa4, NULL, &timer_t4);

    while (true) {
        // watchdog_update(); // Atualiza o watchdog se estiver habilitado

        if (g_flag_permitir_print) {
            g_flag_permitir_print = false; // Consome a flag de impressão

            // --- Exibição no terminal ---
            // Nota: Os tempos de execução individuais (T2, T3, T4) são mais complexos
            // de medir e reportar de forma sincronizada neste modelo com timers separados.
            // Simplificamos o print para mostrar apenas os dados principais.
            printf("Temperatura: %.2f C | Tendencia: %s\n",
                   g_media_temperatura,
                   tendencia_para_texto(g_tendencia_atual));
        }
        
        tight_loop_contents(); 
    }

    return 0; // Nunca alcançado
}
