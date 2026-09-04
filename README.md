# G3X Q10

G3X Q10 é o nome de trabalho de um equalizador paramétrico de dez bandas para
correção cirúrgica e modelagem tonal, em fase de planejamento. A primeira
entrega será um VST3 64-bit para Windows, desenvolvido em C++20 com JUCE e
CMake, seguindo o processo dos plugins G3X.

## Estado

**M1 — seis filtros e validação analítica implementados.**

- [PRD](PRD.md)
- [Referência visual e fontes](docs/references/README.md)
- [Captura da interface de referência](docs/references/waves-q10-equalizer-interface.png)

![Waves Q10 usado como referência de produto](docs/references/waves-q10-equalizer-interface.png)

## Limites da referência

O Waves Q10 foi estudado somente para entender a categoria de equalizadores
paramétricos multibanda e o fluxo combinado de edição gráfica e numérica. O
G3X Q10 terá DSP, marca, código, interface, textos, componentes gráficos e
presets próprios.

Nenhum ativo da Waves será incorporado ao produto final.

## Implementação atual

- Engine biquad C++20 independente de framework e com processamento em `double`.
- Bell, Adaptive Bell, Low Shelf, High Shelf, High Pass e Low Pass.
- Faixas sanitizadas de 10 Hz a `min(30 kHz, 0,475 × sample rate)`, ±24 dB e
  Q de 0,10 a 100.
- Ganho zero exatamente neutro para bells e shelves.
- Consulta da resposta complexa e verificação dos polos de cada filtro.
- Proteção contra parâmetros, entrada e estado numérico inválidos.
- Testes analíticos e de processamento entre 44,1 e 192 kHz.

## Build

```bash
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build --build-config Release --output-on-failure
```

Os testes DSP também podem ser executados sem dependências externas:

```bash
g++ -std=c++20 -Wall -Wextra -Wpedantic -Werror -Isrc \
  src/dsp/BiquadFilter.cpp tests/BiquadFilterTests.cpp -o g3x-q10-tests
./g3x-q10-tests
```
