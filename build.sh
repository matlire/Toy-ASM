

gcc -fsanitize=address,leak,undefined -O2 -Wall -Wextra -Wno-unused-function -lm -D __DEBUG__ -I./libs libs/logging/logging.c libs/instructions_set/instructions_set.c libs/io/io.c libs/stack/stack.c src-compiler/compiler/compiler.c src-compiler/main.c -o dist/compiler.out

gcc -fsanitize=address,leak,undefined -O2 -Wall -Wextra -Wno-unused-function -lm -D __DEBUG__ -I./libs libs/logging/logging.c libs/instructions_set/instructions_set.c libs/io/io.c libs/stack/stack.c src-executor/executor/executor.c src-executor/main.c -o dist/executor.out 

