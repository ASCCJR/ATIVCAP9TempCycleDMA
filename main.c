/**
 * ------------------------------------------------------------
 * Arquivo: main.c
 * Projeto: TempCycleDMA
 * ------------------------------------------------------------
 * Descrição:
 * Ciclo principal do sistema embarcado, baseado em um
 * executor cíclico com tarefas principais:
 *
 * Tarefa 1 - Leitura da temperatura via DMA 
 * Tarefa 2 - Exibição da temperatura e tendência no OLED 
 * Tarefa 3 - Análise da tendência da temperatura 
 * Tarefa 4 - Controle NeoPixel baseado na tendência
 *
 * O sistema utiliza um repeating_timer para iniciar o ciclo
 * de tarefas. As tarefas são executadas sequencialmente
 * após a conclusão da Tarefa 1.
 *
 * MODIFICAÇÃO PRINCIPAL: O ciclo de execução das tarefas agora é
 * disparado por uma flag (g_executar_ciclo_tarefas) que é
 * ativada por um repeating_timer, em vez de um sleep_ms() fixo
 * ao final do loop principal. Isso proporciona um início de ciclo
 * mais preciso e desacopla o tempo de espera da execução das tarefas.
 * ------------------------------------------------------------
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/watchdog.h"
#include "hardware/timer.h"   // Para add_repeating_timer_ms

#include "setup.h"           // Para cfg_temp e setup()
#include "tarefa1_temp.h"
#include "tarefa2_display.h"
#include "tarefa3_tendencia.h"
#include "tarefa4_controla_neopixel.h"
#include "neopixel_driver.h"
#include "testes_cores.h"
#include "pico/stdio_usb.h" 

// Variáveis globais para dados entre tarefas e medição de tempo
float media;
tendencia_t t;
absolute_time_t ini_tarefa1, fim_tarefa1, ini_tarefa2, fim_tarefa2, ini_tarefa3, fim_tarefa3, ini_tarefa4, fim_tarefa4;

// --- MODIFICAÇÃO: Flag global para ser acionada pelo repeating_timer ---
// Esta flag controla quando o bloco principal de tarefas deve ser executado.
volatile bool g_executar_ciclo_tarefas = false;

// --- MODIFICAÇÃO: Callback do repeating_timer ---
// Esta função é chamada pelo hardware timer em intervalos regulares.
// Sua única responsabilidade é sinalizar que um novo ciclo de tarefas deve começar.
bool repeating_timer_callback_principal(struct repeating_timer *timer) {
    (void)timer; // Evita warning de parâmetro não usado se 'timer' não for utilizado no corpo.
    g_executar_ciclo_tarefas = true; // Ativa a flag para o loop principal.
    return true; // Retorna true para que o timer continue repetindo.
}

// Protótipos das funções que encapsulam as chamadas de tarefa originais
// Essas "wrappers" ajudam a organizar o código e manter a lógica de medição de tempo.
void executar_tarefa_1_leitura_temp(void);
void executar_tarefa_2_analise_tendencia(void); 
void executar_tarefa_3_display_oled(void);      
void executar_tarefa_4_controle_neopixel(void);
void executar_tarefa_5_extra_neopixel(void);

int main() {
    setup(); // Chama a função de configuração inicial do hardware e periféricos.

    // --- MODIFICAÇÃO: Configuração do repeating_timer ---
    struct repeating_timer timer_ciclo;
    // Adiciona um timer que chama 'repeating_timer_callback_principal' a cada 1000 ms.
    // O valor -1000 ms significa que o primeiro callback ocorrerá "imediatamente"
    // (após a inicialização do timer) e depois a cada 1000ms.
    // Se a função falhar ao adicionar o timer, imprime um erro e entra em loop infinito.
    if (!add_repeating_timer_ms(-1000, repeating_timer_callback_principal, NULL, &timer_ciclo)) {
        printf("Falha ao adicionar o timer principal!\n");
        while(1) {
            tight_loop_contents(); // Loop de erro simples.
        }
    }

    // --- MODIFICAÇÃO: Loop principal agora é orientado por flag ---
    while (true) { // Loop infinito principal do programa.

        // Verifica se a flag foi ativada pelo callback do timer.
        if (g_executar_ciclo_tarefas) {
            g_executar_ciclo_tarefas = false; // Reseta a flag para o próximo ciclo.

            // Executa as tarefas sequencialmente.
            executar_tarefa_1_leitura_temp();
            executar_tarefa_5_extra_neopixel();
            executar_tarefa_2_analise_tendencia(); 
            executar_tarefa_3_display_oled();      
            executar_tarefa_4_controle_neopixel();

            // Calcula a duração de cada tarefa.
            int64_t tempo1_us = absolute_time_diff_us(ini_tarefa1, fim_tarefa1);
            int64_t tempo2_us = absolute_time_diff_us(ini_tarefa2, fim_tarefa2); 
            int64_t tempo3_us = absolute_time_diff_us(ini_tarefa3, fim_tarefa3); 
            int64_t tempo4_us = absolute_time_diff_us(ini_tarefa4, fim_tarefa4);

            // Imprime os resultados e tempos no terminal serial USB.
            printf("Temperatura: %.2f C | T1(Leitura): %.3fs | T_Disp: %.3fs | T_Tend: %.3fs | T_NeoP: %.3fs | Tend: %s\n",
                   media,
                   tempo1_us / 1e6,
                   tempo2_us / 1e6, 
                   tempo3_us / 1e6, 
                   tempo4_us / 1e6,
                   tendencia_para_texto(t));
        } else {
        }
    }

    return 0; 
}

void executar_tarefa_1_leitura_temp() {
    ini_tarefa1 = get_absolute_time(); // Marca o início da tarefa.
    // A variável 'cfg_temp' é global, definida em setup.c e declarada como extern em setup.h.
    // tarefa1_obter_media_temp é bloqueante e leva aproximadamente 0.5 segundos.
    media = tarefa1_obter_media_temp(&cfg_temp, DMA_TEMP_CHANNEL);
    fim_tarefa1 = get_absolute_time(); // Marca o fim da tarefa.
}

void executar_tarefa_2_analise_tendencia() {
    // Mede o tempo da tarefa de análise de tendência.
    ini_tarefa3 = get_absolute_time();
    t = tarefa3_analisa_tendencia(media); // Analisa a tendência com base na média da temperatura.
    fim_tarefa3 = get_absolute_time();
}

void executar_tarefa_3_display_oled() {
    // Mede o tempo da tarefa de exibição no OLED.
    ini_tarefa2 = get_absolute_time();
    tarefa2_exibir_oled(media, t); // Exibe a média e a tendência no display.
    fim_tarefa2 = get_absolute_time();
}

void executar_tarefa_4_controle_neopixel() {
    ini_tarefa4 = get_absolute_time();
    tarefa4_matriz_cor_por_tendencia(t); // Controla a cor dos LEDs NeoPixel com base na tendência.
    fim_tarefa4 = get_absolute_time();
}

void executar_tarefa_5_extra_neopixel() {
    // Tarefa extra para controle dos NeoPixels se a temperatura for baixa.
    // Os 'sleep_ms(100)' aqui pausam a execução de todas as outras atividades.
    // Se esta tarefa for executada, o ciclo total será maior que 1 segundo.
    if (media < 1.0f) { // Comparação de float com 1.0f.
        npSetAll(COR_BRANCA); // COR_BRANCA definida em testes_cores.h
        npWrite();
        sleep_ms(100); // Pausa de 100ms.
        npClear();
        npWrite();
        sleep_ms(100); // Pausa de 100ms.
    }
}
