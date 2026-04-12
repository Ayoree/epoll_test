#!/bin/bash

X=15
MIN_DELAY=0.0      # минимальная пауза в секундах
MAX_DELAY=0.5      # максимальная пауза в секундах

echo "Запускаем $X параллельных nc клиентов со случайной паузой между командами..."
echo "Диапазон пауз: ${MIN_DELAY} — ${MAX_DELAY} секунд"

for i in $(seq 1 $X); do
    (
        while IFS= read -r cmd || [ -n "$cmd" ]; do
            echo "$cmd"
            RANDOM_DELAY=$(awk -v min=$MIN_DELAY -v max=$MAX_DELAY \
                           'BEGIN{srand(); printf "%.3f", min + rand()*(max-min)}')
            sleep "$RANDOM_DELAY"
            
        done < commands.txt

    ) | nc -q 5 127.0.0.1 6969 > "output/output_${i}.txt" 2>&1 &

    echo "Запущен клиент #$i (PID $!)"
    
    # Небольшая задержка между запуском клиентов
    sleep 0.2
done

wait
echo "Все $X клиентов завершили работу."