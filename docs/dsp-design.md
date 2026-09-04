# Projeto do DSP

## Topologia

O M1 usa biquads em forma transposta direta II e precisão `double`. Os cinco
coeficientes normalizados alimentam tanto o processamento quanto a resposta
complexa usada pelos testes e, futuramente, pelo gráfico da interface.

Os tipos Bell, Low Shelf, High Shelf, High Pass e Low Pass partem das equações
públicas do Audio EQ Cookbook de Robert Bristow-Johnson. A implementação, a API,
os limites, as proteções e os testes foram escritos especificamente para o G3X.

## Adaptive Bell

O Adaptive Bell é uma variação própria. Ele mantém o ganho solicitado na
frequência central e calcula:

```text
Q efetivo = min(100, Q × (1 + abs(ganho em dB) / 12))
```

Assim, ajustes de maior intensidade ficam progressivamente mais estreitos sem
depender da curva ou dos coeficientes de outro produto.

## Segurança

- Sample rate é limitado entre 8 e 384 kHz para o cálculo.
- Frequência é limitada entre 10 Hz e `min(30 kHz, 0,475 × sample rate)`.
- Ganho é limitado a ±24 dB e Q entre 0,10 e 100.
- Valores não finitos recebem padrões seguros.
- Coeficientes instáveis são substituídos por identidade.
- Entrada ou estado não finito é zerado e o estado interno é reiniciado.

## Referência pública

- Robert Bristow-Johnson, *Audio EQ Cookbook*, publicado originalmente no
  arquivo público da Audio Engineering Society.
