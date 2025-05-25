/**
 * ------------------------------------------------------------
 *  Arquivo: main.c
 *  Projeto: TempCycleDMA
 * ------------------------------------------------------------
 *  Descrição:
 *      Ciclo principal do sistema embarcado, baseado em um
 *      executor cíclico com 3 tarefas principais:
 *
 *      Tarefa 1 - Leitura da temperatura via DMA (meio segundo)
 *      Tarefa 2 - Exibição da temperatura e tendência no OLED
 *      Tarefa 3 - Análise da tendência da temperatura
 *
 *      O sistema utiliza watchdog para segurança, terminal USB
 *      para monitoramento e display OLED para visualização direta.
 *
 *  Data: 12/05/2025
// ------------------------------------------------------------
 * 
 *Resumo das alterações:
- A **Tarefa 1** agora é executada por um timer periódico (`add_repeating_timer_ms`).
- As demais tarefas (2, 3, 4) são executadas no loop principal, sempre que 
- uma nova média de temperatura estiver pronta (`nova_temp_pronta`).
- O cálculo dos tempos de execução foi mantido para cada tarefa.
- As funções tarefa_1, tarefa_2, tarefa_3, tarefa_4, tarefa_5
- não são mais necessárias e foram removidas para simplificar o fluxo.
 * 
 */
// ------------------------------------------------------------

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/watchdog.h"
#include "hardware/timer.h"

#include "setup.h"
#include "tarefa1_temp.h"
#include "tarefa2_display.h"
#include "tarefa3_tendencia.h"
#include "tarefa4_controla_neopixel.h"
#include "neopixel_driver.h"
#include "testes_cores.h"  
#include "pico/stdio_usb.h"

float media;
tendencia_t t;
volatile bool nova_temp_pronta = false;

// --- Timer callback para Tarefa 1 ---
bool tarefa1_timer_callback(repeating_timer_t *rt) {
    static absolute_time_t ini_tarefa1, fim_tarefa1;
    ini_tarefa1 = get_absolute_time();
    media = tarefa1_obter_media_temp(&cfg_temp, DMA_TEMP_CHANNEL);
    fim_tarefa1 = get_absolute_time();
    nova_temp_pronta = true;
    return true; // repete
}

int main() {
    setup();

    // Ativa o watchdog com timeout de 2 segundos
    //watchdog_enable(2000, 1);

    repeating_timer_t timer1;
    add_repeating_timer_ms(1000, tarefa1_timer_callback, NULL, &timer1);

    while (true) {
        //watchdog_update();

        if (nova_temp_pronta) {
            nova_temp_pronta = false;

            absolute_time_t ini_tarefa3 = get_absolute_time();
            t = tarefa3_analisa_tendencia(media);
            absolute_time_t fim_tarefa3 = get_absolute_time();

            absolute_time_t ini_tarefa2 = get_absolute_time();
            tarefa2_exibir_oled(media, t);
            absolute_time_t fim_tarefa2 = get_absolute_time();

            absolute_time_t ini_tarefa4 = get_absolute_time();
            tarefa4_matriz_cor_por_tendencia(t);
            absolute_time_t fim_tarefa4 = get_absolute_time();

            // --- Exibição no terminal ---
            printf("Temperatura: %.2f °C | T2: %.3fs | T3: %.3fs | T4: %.3fs | Tendência: %s\n",
                   media,
                   absolute_time_diff_us(ini_tarefa2, fim_tarefa2) / 1e6,
                   absolute_time_diff_us(ini_tarefa3, fim_tarefa3) / 1e6,
                   absolute_time_diff_us(ini_tarefa4, fim_tarefa4) / 1e6,
                   tendencia_para_texto(t));
        }

        tight_loop_contents();
    }

    return 0;
}