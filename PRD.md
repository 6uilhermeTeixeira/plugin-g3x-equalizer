# G3X Equalizer — Product Requirements Document

| Campo | Definição |
| --- | --- |
| Versão | 0.1.0 |
| Status | M2 implementado; M3 planejado |
| Target inicial | C++20, JUCE fixado, CMake |
| Entrega inicial | VST3 64-bit para Windows; Standalone para desenvolvimento |

## 1. Visão do produto

G3X Equalizer é o nome de trabalho de um equalizador paramétrico transparente com até
dez bandas independentes. Ele combina edição direta no gráfico, valores
numéricos precisos e uma visão focada para resolver ressonâncias, limpar o
espectro e realizar modelagem tonal ampla sem impor coloração.

O conceito de produto é informado pelo Waves Q10, cuja documentação descreve
dez bandas, seis famílias de filtro, componentes mono e estéreo, edição gráfica
e numérica, smoothing e processamento em precisão dupla. Coeficientes, curvas,
topologia, código e interface G3X serão desenvolvidos e validados de forma
independente.

## 2. Objetivos

- Permitir correções estreitas e modelagem tonal ampla no mesmo fluxo.
- Oferecer até dez bandas sem comprometer legibilidade ou precisão.
- Manter resposta neutra quando nenhuma banda estiver ativa.
- Evitar zipper noise, instabilidade e explosões durante automação.
- Operar com zero amostras de latência adicionada.
- Preservar imagem estéreo nos modos vinculados.
- Garantir recall confiável por IDs de parâmetros estáveis.

## 3. Usuários e casos de uso

- Remoção de ressonâncias em voz, instrumentos e gravações de campo.
- Filtros passa-altas e passa-baixas para limpeza espectral.
- Ajustes amplos de timbre em pistas, buses e master.
- Correção independente de desequilíbrios entre canais estéreo.
- Criação de filtros criativos e efeitos de banda limitada.
- Ajustes de sistemas e material ao vivo dentro de margens seguras.

O plugin não é um equalizador dinâmico, linear-phase, match EQ ou analisador de
restauração na primeira versão.

## 4. Bandas e parâmetros

O estado público contém dez slots fixos (`band1` a `band10`). Slots inativos não
processam áudio, mas preservam seus últimos valores para recall e automação.

### 4.1 Estado (`bandN.enabled`)

- Liga ou desliga cada banda com transição suavizada.
- Padrão: desligado.
- Atalho de solo auditivo será apenas uma função de UI, não automatizável.

### 4.2 Frequência (`bandN.frequencyHz`)

- Faixa: 10 Hz até o menor valor entre 30 kHz e 0,475 × sample rate.
- Mapeamento logarítmico no gráfico e no controle rotativo.
- Entrada numérica em Hz/kHz e limites adaptados ao sample rate.

### 4.3 Ganho (`bandN.gainDb`)

- Faixa: -24.0 a +24.0 dB.
- Resolução exibida: 0.1 dB.
- Padrão: 0.0 dB.
- Ignorado por filtros passa-altas e passa-baixas.

### 4.4 Largura (`bandN.q`)

- Faixa: 0.10 a 100.0.
- Mapeamento logarítmico para preservar precisão útil.
- Limites específicos por tipo quando exigidos por estabilidade.

### 4.5 Tipo (`bandN.type`)

- `Bell`: ganho simétrico em torno da frequência central.
- `Adaptive Bell`: largura diminui progressivamente com o ganho absoluto.
- `Low Shelf`: reforço ou corte abaixo da transição.
- `High Shelf`: reforço ou corte acima da transição.
- `High Pass`: atenuação abaixo da frequência.
- `Low Pass`: atenuação acima da frequência.

`Adaptive Bell` é uma proposta própria de interação. Sua relação ganho/largura
será definida por testes e não copiará a curva proportional-Q da referência.

## 5. Canais e vínculo

### 5.1 Modos

- `Mono`: uma cadeia de até dez bandas.
- `Stereo Linked`: mesmos parâmetros nos dois canais.
- `Dual Mono`: parâmetros L/R independentes para cada banda.

### 5.2 Vínculo (`stereoLink`)

- Padrão: ligado no componente estéreo.
- Ao desvincular, ambos os canais recebem uma cópia do estado atual.
- Ao vincular estados diferentes, a UI solicitará `usar L`, `usar R` ou
  `manter diferenças e vincular próximos movimentos`.
- Automação deve permanecer determinística após mudanças de modo.

## 6. Entrada, saída e precisão

- `InputGain`: -24 a +12 dB; padrão 0 dB.
- `OutputGain`: -24 a +12 dB; padrão 0 dB.
- Medidores peak por canal antes e depois do processamento.
- Indicador de clipping com retenção e reset pelo usuário.
- Processamento interno em `double` no release inicial.
- Bypass geral com transição curta e sem clique.

## 7. Processamento proposto

```text
Input
  -> segurança contra denormals/NaN/Inf
  -> input gain
  -> banda 1 ... banda 10 (IIR estável, precisão dupla)
  -> output gain
  -> medição Peak e proteção numérica
  -> Output
```

### 7.1 Filtros

- Biquads baseados em formulações públicas e documentadas.
- Coeficientes recalculados fora do loop por amostra quando possível.
- Interpolação ou morph estável durante mudanças contínuas.
- Polos verificados dentro do círculo unitário em toda a faixa suportada.
- Ganho exatamente unitário para bandas bell/shelf em 0 dB, dentro da
  tolerância numérica definida pelos testes.

No M1, os seis tipos usam biquads em forma transposta direta II, coeficientes
normalizados e processamento em precisão dupla. A resposta complexa e os polos
são expostos pela mesma estrutura de coeficientes usada no áudio. O Adaptive
Bell aplica a relação própria `Q efetivo = Q × (1 + |ganho| / 12)`, limitada a
100, preservando o ganho central enquanto estreita a largura com a intensidade.

### 7.2 Filtros de corte

- Inclinação inicial: 12 dB/oitava por banda.
- Bandas sobrepostas podem formar inclinações mais fortes de maneira explícita.
- Frequências próximas de Nyquist serão limitadas para manter estabilidade.
- Sem oversampling ou linear phase na primeira versão.

### 7.3 Automação

- Smoothing em frequência, ganho, Q, input e output.
- Mudança de tipo realizada por crossfade curto entre topologias.
- Liga/desliga da banda sem descontinuidade.
- Nenhum parâmetro pode gerar NaN, Inf ou saída não limitada por erro numérico.

O M2 implementa ramps lineares de 20 ms para frequência, ganho, Q, enable,
input, output e bypass. Mudanças de tipo usam dois biquads e crossfade antes de
promover a nova topologia. A cadeia usa armazenamento fixo de dez bandas por
canal e não aloca memória durante o processamento.

## 8. Interface proposta

- Gráfico logarítmico de resposta combinada com grade de frequência e ganho.
- Dez pontos numerados e identificados por paleta acessível a daltonismo.
- Arraste horizontal para frequência e vertical para ganho.
- Gestos/modificadores para Q, ajuste fino e restrição de eixo.
- Painel focado com frequência, ganho, Q, tipo e estado da banda selecionada.
- Visão de tabela opcional para comparar todas as bandas.
- Entrada numérica com unidades e validação imediata.
- Curvas individuais discretas e curva total em maior contraste.
- Medidores, ganhos globais, vínculo e bypass fora do gráfico.
- Janela redimensionável, HiDPI, teclado e leitor de tela.
- Identidade G3X original, sem copiar layout, cores, tipografia, ícones,
  proporções ou aparência da referência.

## 9. Resposta visual

- Curva calculada a partir dos mesmos coeficientes usados no áudio.
- Atualização visual desacoplada da thread de áudio.
- Resolução suficiente para representar Q alto sem ocultar picos estreitos.
- Marcadores fora da faixa visível devem continuar selecionáveis pela tabela.
- Escala vertical selecionável entre ±12, ±24 e ±36 dB.

## 10. Requisitos de tempo real

- Nenhuma alocação, mutex, I/O, logging ou chamada de UI em `processBlock`.
- Funcionamento de 44.1 a 192 kHz e buffers de 16 a 2048 samples.
- Zero amostras de latência adicionada e reportada ao host.
- Estado serializado, versionado e compatível entre versões.
- Meta inicial em 48 kHz/64 samples: até 1% de um núcleo moderno por instância
  estéreo com dez bandas ativas, a calibrar em hardware de referência.

## 11. Presets iniciais

- Flat
- Vocal Cleanup
- Resonance Search
- Low Rumble Control
- Gentle Presence
- Broad Mix Shape
- Telephone Band

Os presets serão desenvolvidos do zero e conterão apenas parâmetros G3X.

## 12. Estratégia de validação

### 12.1 Testes automatizados

- Estado flat: null/identidade dentro da tolerância de ponto flutuante.
- Resposta por impulso comparada à resposta analítica de cada filtro.
- Frequência central, ganho e Q medidos em pontos representativos.
- Estabilidade de polos em combinações extremas e todos os sample rates.
- Empilhamento de dez bandas sem NaN/Inf ou overflow.
- Continuidade durante sweeps e mudanças de tipo.
- Paridade L/R em `Stereo Linked`.
- Independência entre canais em `Dual Mono`.
- Recall exato de parâmetros, IDs e estado de vínculo.

### 12.2 Testes auditivos

- Voz, baixo, bateria, guitarras, piano, ruído e mix completa.
- Boost/cut estreito, shelves amplos e filtros empilhados.
- Automação lenta e rápida de frequência, ganho e Q.
- Comparação equal-loudness e verificação de clicks/zipper noise.
- Operação em hosts com buffers pequenos e bounce offline.

### 12.3 Referência externa

O Waves Q10 poderá ser usado somente como referência de categoria e fluxo. O
objetivo não é correspondência sample-a-sample, engenharia reversa ou clonagem
de suas curvas, coeficientes, presets e comportamento.

## 13. Critérios de aceitação do protótipo

- Build Debug e Release em Linux; Release VST3 em Windows CI com MSVC.
- Dez bandas simultâneas com seis tipos próprios e resposta estável.
- Estado flat comprovadamente neutro e zero latência reportada.
- Automação sem clicks, instabilidade, NaN/Inf ou divergência de canais.
- Gráfico coerente com medições da saída dentro da tolerância.
- Estado recuperado após salvar e reabrir o host.
- Aprovação no pluginval/VST3 Validator antes do beta.
- Validação manual no FL Studio em Windows.

## 14. Fora do escopo inicial

- Reprodução exata ou engenharia reversa do Waves Q10.
- Uso de marca, código, assets, presets ou trade dress da Waves.
- EQ dinâmico, linear-phase, minimum/linear mix, match EQ e processamento M/S.
- Analisador FFT em tempo real no M1.
- AAX, formatos nativos de DAWs, iOS e versão final para macOS.

## 15. Marcos propostos

1. **M0 — Fundação (concluído):** PRD, naming e arquitetura.
2. **M1 — Filtros (concluído):** seis tipos, precisão dupla, resposta complexa,
   verificação de polos e testes analíticos/processados.
3. **M2 — Produto (concluído):** dez bandas, estéreo vinculado/dual-mono,
   automação suavizada, bypass, ganhos globais e wrapper JUCE com estado.
4. **M3 — Interface:** gráfico, tabela, foco e acessibilidade.
5. **M4 — Windows Alpha:** CI MSVC, VST3, validadores e FL Studio.
6. **M5 — Beta:** presets, performance, regressão e empacotamento.

## 16. Decisões que precisam de confirmação

- Confirmar `G3X Equalizer` como nome público ou manter apenas como codinome,
  considerando a proximidade com a marca Waves Q10.
- Manter ±24 dB ou reduzir o ganho para uma faixa mais conservadora.
- Semântica exata do vínculo ao reunir canais com estados diferentes.
- Adicionar analisador espectral depois do protótipo ou manter foco no EQ.
- Orçamento de CPU e política para instâncias com bandas inativas.
- Modelo de licença e compatibilidade com a licença do JUCE.

## 17. Fontes de pesquisa

- [Página oficial do Waves Q10](https://www.waves.com/plugins/q10-equalizer)
- [Manual oficial](https://assets.wavescdn.com/pdf/plugins/q10-equalizer.pdf)
- [Imagem oficial da interface](https://media.wavescdn.com/images/products/plugins/600/q10-equalizer.png)
- [Tutorial oficial](https://www.waves.com/how-to-eq-q10-paragraphic-equalizer)

Consulta realizada em 4 de setembro de 2026. As fontes são documentação de
referência; não constituem especificação de clonagem.
