# Mooncake: глобальный многоуровневый KV-кэш

**Проектное задание:** [P29. Mooncake: глобальный многоуровневый KV-кэш](https://distsys.ru/osukhoroslov/nis2/src/branch/main/project-tasks/p29-mooncake.md)

**Участники:**

- Кирилл Агафонов
- Артем Акимов
- Альберт Жемуков

## Сокращённый запуск

### Требования

- macOS
- Homebrew
- Apple Clang 17
- CMake 3.24+
- Ninja
- Python 3.14
- OpenSSL 3
- curl

### Установка зависимостей

```bash
brew install cmake ninja boost fmt yaml-cpp libev c-ares cctz openssl@3 curl nghttp2 \
  icu4c zlib re2 cryptopp abseil googletest google-benchmark clang-format pkgconf python@3.14
```

Чтобы собрать проект, нужно отвязать `openssl@4`, с ним userver не собирается:

```bash
brew unlink openssl@4
```

### Сборка

Сборка занимает пару минут, userver подтянет CMake.

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DUSERVER_FEATURE_CCACHE=OFF \
  -DOPENSSL_ROOT_DIR="$(brew --prefix openssl@3)" \
  -DUSERVER_PYTHON_PATH="$(brew --prefix python@3.14)/bin/python3.14" \
  -DCMAKE_EXE_LINKER_FLAGS="-L$(brew --prefix icu4c)/lib" \
  -DCMAKE_PREFIX_PATH="$(brew --prefix curl);$(brew --prefix icu4c);$(brew --prefix zlib)"
```

### Тесты

Собирает узел, гоняет unit-тесты, поднимает два узла и проверяет запись блока, чтение из памяти и с диска и чтение через соседний узел:

```bash
scripts/check_node.sh
```

### Проверка трассы

```bash
scripts/fetch_trace.sh
scripts/trace_stats.py data/traces/conversation_trace.jsonl --limit 200
```

Что должно получиться, описано в [КТ-1](nis/checkpoints/01.md#технические-проверки).

## Навигация

- [Описание проекта](nis/project.md)
- [План эксперимента](nis/experiment-plan.md)
- [Контрольная точка 1](nis/checkpoints/01.md)
- [Контрольная точка 2](nis/checkpoints/02.md)
- [Контрольная точка 3](nis/checkpoints/03.md)
- [Состояние проекта](nis/status.md)
- [Результаты](nis/results.md)
- [Финальная версия](nis/final.md)
- [Код](src/)
- [Конфиги](configs/)
- [Скрипты](scripts/)
- [Тесты](tests/)
- [Данные](scripts/fetch_trace.sh)
- [Результаты проверок КТ-1](artifacts/checkpoints/01/)
