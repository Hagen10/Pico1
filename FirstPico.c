#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include <string.h>

#define YELLOW_LED 16
#define SWITCH 5
#define RED_LED 15

const uint32_t EDGE_DEBOUNCE_US = 30000;       // 30 ms
const uint32_t DOT_DASH_THRESHOLD_US = 500000; // 500 ms
const uint32_t LETTER_GAP_US = 2000000;
#define MESSAGE_TIMEOUT_MS 5000
#define MORSE_WORD_SPACE_MS 4000
#define MORSE_LETTER_SPACE_MS 1500
#define MORSE_LETTER_SPACE_FLASH_MS 500
#define MORSE_DASH_MS 1500
#define MORSE_DOT_MS 300
#define MORSE_BREAK_MS 750

#define MORSE_LETTER_SEPARATOR ' '

volatile absolute_time_t last_edge_time = {0};
volatile absolute_time_t press_start_time = {0};
volatile absolute_time_t last_symbol_time = {0};
volatile bool waiting_for_release = false;
volatile bool message_ready = false;
volatile bool question_cancelled = false;
volatile bool letter_gap_indicated = false;
volatile bool morse_updated = false;

char morse_buffer[32];
volatile int morse_len = 0;

typedef struct
{
    const char *text;
    const char *morse;
} MorseLetter;

typedef struct
{
    const char *question;
    const char *answer;
} MorseQuestion;

// Questions can be changed here. Answers are written as Morse symbols.
const MorseQuestion questions[] = {
    {"Hvem elsker at fluefiske?", ".--. .- .-.. .-.. ."},
    {"Hvem har boet 4 år i USA?", ".-.. .- .-. ..."},
    {"Hvem er barn nr 7 i familien?", ".-. --- . .-.."},
};

const MorseLetter morse_alphabet[] = {
    {"a", ".-"}, 
    {"b", "-..."}, 
    {"c", "-.-."}, 
    {"d", "-.."},
    {"e", "."}, 
    {"f", "..-."}, 
    {"g", "--."}, 
    {"h", "...."},
    {"i", ".."}, 
    {"j", ".---"}, 
    {"k", "-.-"}, 
    {"l", ".-.."},
    {"m", "--"}, 
    {"n", "-."}, 
    {"o", "---"}, 
    {"p", ".--."},
    {"q", "--.-"}, 
    {"r", ".-."}, 
    {"s", "..."}, 
    {"t", "-"},
    {"u", "..-"}, 
    {"v", "...-"}, 
    {"w", ".--"}, 
    {"x", "-..-"},
    {"y", "-.--"}, 
    {"z", "--.."}, 
    {"æ", ".-.-"}, 
    {"ø", "---."},
    {"å", ".--.-"}, 
    {"0", "-----"}, 
    {"1", ".----"}, 
    {"2", "..---"},
    {"3", "...--"}, 
    {"4", "....-"}, 
    {"5", "....."}, 
    {"6", "-...."},
    {"7", "--..."}, 
    {"8", "---.."}, 
    {"9", "----."}, 
    {"?", "..--.."},
};

const size_t question_count = sizeof(questions) / sizeof(questions[0]);
int current_question = 0;

void switch_pressed(uint gpio, uint32_t event_mask)
{
    if (gpio != SWITCH)
        return;

    absolute_time_t now = get_absolute_time();
    uint64_t dt = absolute_time_diff_us(last_edge_time, now);

    if (dt < EDGE_DEBOUNCE_US)
        return;
    last_edge_time = now;

    // Read the settled pin level so a callback containing both edge flags
    // cannot accidentally discard the press or release.
    if (!gpio_get(SWITCH) && (event_mask & GPIO_IRQ_EDGE_FALL))
    {
        question_cancelled = true;
        pwm_set_gpio_level(YELLOW_LED, 0);
        press_start_time = now;
        pwm_set_gpio_level(RED_LED, 65000);
        waiting_for_release = true;
        return;
    }

    if (gpio_get(SWITCH) && (event_mask & GPIO_IRQ_EDGE_RISE))
    {
        pwm_set_gpio_level(RED_LED, 0);

        if (waiting_for_release)
        {
            uint64_t press_us = absolute_time_diff_us(press_start_time, now);

            if (morse_len < (int)sizeof(morse_buffer) - 1)
            {
                morse_buffer[morse_len++] = press_us < DOT_DASH_THRESHOLD_US ? '.' : '-';
                morse_buffer[morse_len] = '\0';
                morse_updated = true;
                last_symbol_time = now;
                letter_gap_indicated = false;
            }

            waiting_for_release = false;
        }
    }
}

void blink_symbol(bool dash)
{
    pwm_set_gpio_level(YELLOW_LED, 65000);
    for (uint32_t elapsed_ms = 0; elapsed_ms < (dash ? MORSE_DASH_MS : MORSE_DOT_MS); elapsed_ms += 20)
    {
        if (question_cancelled)
        {
            pwm_set_gpio_level(YELLOW_LED, 0);
            return;
        }
        sleep_ms(20);
    }
    pwm_set_gpio_level(YELLOW_LED, 0);
    for (uint32_t elapsed_ms = 0; elapsed_ms < MORSE_BREAK_MS; elapsed_ms += 20)
    {
        if (question_cancelled)
            return;
        sleep_ms(20);
    }
}

const char *find_morse(const char *letter)
{
    char normalized_letter[4] = {0};
    strncpy(normalized_letter, letter, sizeof(normalized_letter) - 1);
    if (normalized_letter[0] >= 'A' && normalized_letter[0] <= 'Z')
        normalized_letter[0] += 'a' - 'A';

    for (size_t i = 0; i < sizeof(morse_alphabet) / sizeof(morse_alphabet[0]); ++i)
    {
        if (strcmp(normalized_letter, morse_alphabet[i].text) == 0)
            return morse_alphabet[i].morse;
    }
    return NULL;
}

void send_morse_text(const char *text)
{
    for (size_t i = 0; text[i] != '\0';)
    {
        if (text[i] == ' ')
        {
            // for (uint32_t elapsed_ms = 0; elapsed_ms < MORSE_WORD_SPACE_MS; elapsed_ms += 20)
            // {
            //     if (question_cancelled)
            //         return;
            //     sleep_ms(20);
            // }
            ++i;
            continue;
        }

        const char *letter = NULL;
        unsigned char byte = (unsigned char)text[i];
        size_t letter_length = 1;
        if ((byte & 0xe0) == 0xc0)
            letter_length = 2;
        else if ((byte & 0xf0) == 0xe0)
            letter_length = 3;
        char letter_buffer[4] = {0};
        memcpy(letter_buffer, &text[i], letter_length);
        letter = find_morse(letter_buffer);

        if (letter != NULL)
        {
            for (const char *symbol = letter; *symbol; ++symbol)
                blink_symbol(*symbol == '-');
        }

        pwm_set_gpio_level(RED_LED, 65000);
        for (uint32_t elapsed_ms = 0; elapsed_ms < MORSE_LETTER_SPACE_FLASH_MS; elapsed_ms += 20)
        {
            if (question_cancelled)
                return;
            sleep_ms(20);
        }

        pwm_set_gpio_level(RED_LED, 0);

        for (uint32_t elapsed_ms = 0; elapsed_ms < MORSE_LETTER_SPACE_MS; elapsed_ms += 20)
        {
            if (question_cancelled)
                return;
            sleep_ms(20);
        }

        i += letter_length;
    }
}

void ask_current_question(void)
{
    question_cancelled = false;
    printf("Question %d\n", current_question + 1);
    send_morse_text(questions[current_question].question);
}

void flash_result(uint pin, bool pwm)
{
    if (!pwm)
        gpio_put(pin, true);

    for (uint32_t elapsed_ms = 0; elapsed_ms < 3000; elapsed_ms += 250)
    {
        if (pwm)
            pwm_set_gpio_level(pin, (elapsed_ms / 250) % 2 == 0 ? 65000 : 0);
        else
            gpio_put(pin, (elapsed_ms / 250) % 2 == 0);
        sleep_ms(250);
    }

    if (pwm)
        pwm_set_gpio_level(pin, 0);
    else
        gpio_put(pin, false);
}

int main()
{
    // Needed for getting logs from `printf` via USB
    stdio_init_all();

    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    gpio_init(YELLOW_LED);
    gpio_set_function(YELLOW_LED, GPIO_FUNC_PWM);

    gpio_init(RED_LED);
    gpio_set_function(RED_LED, GPIO_FUNC_PWM);

    // Additional PWM CONFIGURATION
    uint slice_num = pwm_gpio_to_slice_num(YELLOW_LED);
    pwm_set_enabled(slice_num, true);

    uint slice2_num = pwm_gpio_to_slice_num(RED_LED);
    pwm_set_enabled(slice2_num, true);

    gpio_init(SWITCH);
    gpio_set_dir(SWITCH, GPIO_IN);
    gpio_pull_up(SWITCH); // Use internal pull-down resistor
    gpio_set_irq_enabled_with_callback(SWITCH, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &switch_pressed);

    ask_current_question();

    while (true)
    {
        absolute_time_t now = get_absolute_time();

        if (morse_updated)
        {
            printf("MORSE: %s\n", morse_buffer);
            morse_updated = false;
        }

        if (morse_len > 0 &&
            absolute_time_diff_us(last_symbol_time, now) <= MESSAGE_TIMEOUT_MS * 1000)
        {
            uint64_t symbol_gap_us = absolute_time_diff_us(last_symbol_time, now);

            // Flash yellow LED when letter gap threshold is reached
            if (symbol_gap_us > LETTER_GAP_US && !letter_gap_indicated)
            {
                pwm_set_gpio_level(YELLOW_LED, 65000);
                letter_gap_indicated = true;
            }
            else if (symbol_gap_us <= LETTER_GAP_US)
            {
                pwm_set_gpio_level(YELLOW_LED, 0);
                letter_gap_indicated = false;
            }

            if (symbol_gap_us > LETTER_GAP_US &&
                     morse_buffer[morse_len - 1] != MORSE_LETTER_SEPARATOR &&
                     morse_len < (int)sizeof(morse_buffer) - 1)
            {
                morse_buffer[morse_len++] = MORSE_LETTER_SEPARATOR;
                morse_buffer[morse_len] = '\0';
            }
        }

        if (morse_len > 0 &&
            absolute_time_diff_us(last_symbol_time, now) > MESSAGE_TIMEOUT_MS * 1000)
        {
            pwm_set_gpio_level(YELLOW_LED, 0);
            letter_gap_indicated = false;
            
            while (morse_len > 0 &&
                   morse_buffer[morse_len - 1] == MORSE_LETTER_SEPARATOR)
            {
                --morse_len;
            }
            morse_buffer[morse_len] = '\0';
            printf("Answer received: %s\n", morse_buffer);
            if (strcmp(morse_buffer, questions[current_question].answer) == 0)
            {
                printf("Correct answer!\n");
                flash_result(PICO_DEFAULT_LED_PIN, false);
                ++current_question;
                if (current_question == (int)question_count)
                {
                    printf("All questions answered.\n");
                    gpio_put(PICO_DEFAULT_LED_PIN, true);
                    current_question = 0;
                    break;
                }
                message_ready = true;
            }
            else
            {
                printf("Incorrect answer. Repeating question.\n");
                message_ready = true;
                flash_result(RED_LED, true);
            }
            morse_len = 0;
        }

        if (message_ready)
        {
            ask_current_question();
            message_ready = false;
        }

        sleep_ms(50);
    }
}
