<p align="center"><img src="resources/owlio.png" width="112" alt="Owlio"></p>

# Owlio

*[Read in English](README.md)*

Editor de roteiros no estilo Final Draft, em C++17 e Qt 6.

## Recursos

- **Páginas reais na tela**: Courier 12, US Letter, margens no padrão da indústria (1,5" à esquerda, 1" nas outras). O que você vê é exatamente o que sai no PDF.
- **6 elementos**: Cabeçalho, Ação, Personagem, Parentético, Diálogo, Transição.
- **Fluxo do Final Draft**: Enter e Tab escolhem o próximo elemento sozinhos; Ctrl+1…6 define o elemento.
- **Autocompletar (SmartType)**: nomes de personagens, locações, horários (DIA, NOITE, …), extensões (V.O., O.S.) e transições.
- **Paginação profissional**: cabeçalho e personagem nunca ficam sozinhos no pé da página, falas quebradas recebem (MAIS) / NOME (CONT.) (há opção para MORE/CONT'D), e as páginas são numeradas.
- **Arquivos**: Fountain (`.fountain`, formato nativo, texto puro) e Final Draft (`.fdx`, abrir e salvar).
- **Página de rosto** editada numa janela com prévia ao vivo: título, crédito, autores, fonte, versão, data, observações e contato.
- **Janela de impressão e PDF** com miniaturas das folhas: impressora, cópias, intervalo de páginas, página de rosto, numeração de cenas e **marca d'água** opcional (por exemplo "RASCUNHO" ou o nome de quem recebe a cópia).
- Navegador de cenas, lista de personagens, localizar/substituir, desfazer/refazer, numeração de cenas e zoom.

## Tela de cartões

**Exibir → Cartões** (Ctrl+Shift+C) mostra cada cena como um cartão num quadro:

- Número, INT/EXT, local, horário (com cor: dia, noite, amanhecer/anoitecer, contínuo), página onde a cena começa, quem fala nela e o tamanho em oitavos de página.
- **Sinopse**: F2 ou clique direito → Editar sinopse. Sem sinopse, o cartão mostra o começo da ação. A sinopse é salva no Fountain como `= texto` logo abaixo do cabeçalho, e no `.fdx` como resumo da cena (`SceneProperties/Summary`).
- **Reordenar**: arraste o cartão, ou use Alt+setas. A cena inteira muda de lugar no roteiro, e Ctrl+Z desfaz.
- Duplo clique ou Enter abre a cena no roteiro. Ctrl+roda do mouse muda o tamanho dos cartões.

## Visual e temas

- Tema **claro** e **escuro**, seguindo o sistema por padrão (Exibir → Tema).
- Ícones SVG próprios (`resources/icons`), recoloridos automaticamente para cada tema.
- Painéis em cartões: cenas com número, página e quem fala, e personagens com avatar.
- **Modo foco** (Ctrl+Shift+F): esmaece tudo menos o trecho atual. Numa fala, o personagem, os parentéticos e o diálogo ficam em destaque juntos.
- **Fotos dos personagens**: duplo clique num personagem (ou clique direito → Escolher foto). A foto aparece no painel e nos cartões, e fica guardada dentro do próprio `.fountain`, num bloco de comentário que outros apps Fountain ignoram.

O estilo fica em [`resources/themes/estilo.qss`](resources/themes/estilo.qss). É CSS do Qt com variáveis de cor (`@accent`, `@surface`, …), definidas em `src/theme.cpp`. Para editar e ver o resultado ao salvar, sem recompilar:

```bash
OWLIO_QSS=resources/themes/estilo.qss ./run.sh
```

Para forçar um tema numa execução: `./run.sh --tema escuro`.

## Compilar

Requer CMake ≥ 3.16, g++ com C++17 e Qt 6 (Widgets, PrintSupport, Svg; Test para o teste de interface).

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=$HOME/Qt/6.8.3/gcc_64
cmake --build build -j
```

Com o Qt instalado pelo sistema (`sudo apt install qt6-base-dev libxcb-cursor0`), não é preciso passar `CMAKE_PREFIX_PATH`.

## Executar

```bash
./run.sh exemplos/a_ultima_sessao.fountain
```

O `run.sh` aponta para o Qt baixado em `~/Qt` e para a `libxcb-cursor0` extraída localmente.

Linha de comando:

```bash
./run.sh roteiro.fountain --export-pdf roteiro.pdf
```

## Testes

```bash
./build/owlio_tests          # núcleo: Fountain, FDX, paginação, edição
./run.sh --help
LD_LIBRARY_PATH=$HOME/Qt/6.8.3/gcc_64/lib:$HOME/Qt/extra/usr/lib/x86_64-linux-gnu ./build/owlio_gui_test
```

## Estrutura

| Arquivo | Papel |
|---|---|
| `src/document.*` | Modelo (parágrafos e elementos), UTF-8/UTF-32 |
| `src/layout.*` | Quebra de linha e paginação (grade de 10 col × 6 linhas por polegada) |
| `src/editor.*` | Cursor, seleção, regras de Tab/Enter, autocompletar, desfazer, localizar |
| `src/fountain.*`, `src/fdx.*` | Leitura e escrita de arquivos |
| `src/pagepainter.*` | Desenha as páginas em tela, PDF ou impressora (`QGlyphRun` em posições exatas) |
| `src/scriptview.*` | Widget de edição (teclado, mouse, método de entrada, rolagem) |
| `src/mainwindow.*` | Menus, barra de ferramentas, painéis |
| `src/titlepagedialog.*`, `src/printdialog.*` | Janelas de página de rosto e de impressão/PDF, com prévia |
| `src/avatars.*` | Fotos dos personagens: importação, recorte, cache e desenho em círculo |
| `src/theme.*` | Paleta, QSS com variáveis, ícones SVG recoloridos, claro/escuro |
| `src/carddelegate.*` | Cartões dos painéis de cenas e personagens |
| `src/sceneboard.*` | Tela de cartões (quadro de cenas com arrastar e soltar) |

O núcleo (`document`, `layout`, `editor`, `fountain`, `fdx`) não depende do Qt.
