<p align="center"><img src="resources/owlio.png" width="112" alt="Owlio"></p>

# Owlio

*[Read in English](README.md)*

Editor de roteiros no estilo Final Draft, em C++17 e Qt 6.

O idioma oficial do Owlio é o inglês. A interface em **português do Brasil** está em **Editar → Configurações** (Ctrl+,): escolha o idioma e o Owlio reinicia já traduzido. Para uma execução só: `./run.sh --language pt_BR`.

## Recursos

- **Páginas reais na tela**: Courier 12, US Letter, margens no padrão da indústria (1,5" à esquerda, 1" nas outras). O que você vê é exatamente o que sai no PDF.
- **6 elementos**: Cabeçalho, Ação, Personagem, Parentético, Diálogo, Transição.
- **Fluxo do Final Draft**: Enter e Tab escolhem o próximo elemento sozinhos; Ctrl+1…6 define o elemento.
- **Diálogo duplo**: duas falas lado a lado, quando os personagens falam ao mesmo tempo. Com o cursor em qualquer uma das falas, use Formatar → Diálogo duplo (Ctrl+Shift+D). No Fountain fica salvo como `^`, no Final Draft como `<DualDialogue>`.
- **Autocompletar (SmartType)**: nomes de personagens, locações, horários, extensões (V.O., O.S.) e transições.
- **Paginação profissional**: cabeçalho e personagem nunca ficam sozinhos no pé da página, falas quebradas recebem (MORE) / NOME (CONT'D), e as páginas são numeradas.
- **Convenções do roteiro** (Configurações): inglês, com (MORE)/(CONT'D), DAY, NIGHT e CUT TO:, ou português, com (MAIS)/(CONT.), DIA, NOITE e CORTA PARA:. Valem para a paginação e para as sugestões do autocompletar, independente do idioma da interface.
- **Arquivos**: Fountain (`.fountain`, formato nativo, texto puro) e Final Draft (`.fdx`, abrir e salvar).
- **Página de rosto** editada numa janela com prévia ao vivo: título, crédito, autores, fonte, versão, data, observações e contato. O título sai do jeito que você digita, com as maiúsculas e minúsculas que escolher.
- **Janela de impressão e PDF** com miniaturas das folhas: impressora, cópias, intervalo de páginas, página de rosto, numeração de cenas e **marca d'água** opcional (por exemplo "RASCUNHO" ou o nome de quem recebe a cópia).
- Navegador de cenas, lista de personagens, localizar/substituir, desfazer/refazer, numeração de cenas e zoom.

## Salvamento e backups

- **Salvamento automático**: roteiros que já têm arquivo são salvos sozinhos a cada 5 minutos (ou 1, 10, ou desligado, nas Configurações).
- **Recuperação**: enquanto houver alterações não salvas, o Owlio guarda uma cópia a cada 20 segundos, inclusive de roteiros sem título. Se ele fechar de repente, oferece as alterações de volta no próximo início.
- **Backups**: cada salvamento guarda também uma cópia com data e hora, até as 30 mais recentes de cada roteiro. Salvamentos automáticos geram no máximo um backup a cada 10 minutos. Arquivo → Abrir arquivo de backup abre um deles como cópia sem título, para o backup nunca ser sobrescrito.
- **Versões** (Arquivo → Versões, Ctrl+Shift+H, ou o botão de relógio na barra de ferramentas): uma máquina do tempo para o roteiro. Ela abre sobre a área do roteiro; em Configurações → Salvamento dá para abrir em tela cheia. Cada versão salva aparece empilhada em perspectiva sob um céu noturno, a mais nova na frente, com uma linha do tempo ao lado. Volte com as setas, a roda do mouse ou a linha do tempo, folheie com ← e → e clique em **Restaurar** para trazer aquela versão de volta. A restauração é uma edição comum, então Ctrl+Z volta para onde você estava.

## Tela de cartões

**Exibir → Cartões** (Ctrl+Shift+C) mostra cada cena como um cartão num quadro:

- Número, INT/EXT, local, horário (com cor: dia, noite, amanhecer/anoitecer, contínuo), página onde a cena começa, quem fala nela e o tamanho em oitavos de página.
- **Sinopse**: F2 ou clique direito → Editar sinopse. Sem sinopse, o cartão mostra o começo da ação. A sinopse é salva no Fountain como `= texto` logo abaixo do cabeçalho, e no `.fdx` como resumo da cena (`SceneProperties/Summary`).
- **Reordenar**: arraste o cartão, ou use Alt+setas. A cena inteira muda de lugar no roteiro, e Ctrl+Z desfaz.
- Duplo clique ou Enter abre a cena no roteiro. Ctrl+roda do mouse muda o tamanho dos cartões.
- **Grade ou lista**: um botão no canto do cabeçalho alterna entre a grade de cartões e uma lista compacta, com uma faixa por cena (local, horário, sinopse, personagens, duração e página). Arrastar, setas e F2 funcionam nos dois modos, e o Owlio lembra a escolha.

## Linha do tempo

**Exibir → Linha do tempo** (Ctrl+Shift+L) mostra o filme da esquerda para a direita: régua de páginas, estrutura da história, uma faixa com os blocos das cenas e uma linha por personagem mostrando onde ele fala. Passe o mouse numa cena para ver página e duração; clique para abri-la no roteiro.

- Compare o que já está escrito com a **duração planejada**, ou aproxime só as páginas escritas.
- **Estrutura editável**: arraste divisões de atos e pontos de virada, renomeie, divida ou junte atos, e adicione ou remova pontos pelo clique direito.
- **Modelos**: Convencional (três atos), Fragmentada (não linear), Episódio de TV (drama), Sitcom, Cinco atos (Freytag), Save the Cat, Jornada do herói e Kishōtenketsu. Qualquer edição transforma a estrutura em Personalizada. Ela fica salva no `.fountain`, e toda mudança pode ser desfeita.

## Mapa de relações

**Exibir → Relações** (Ctrl+Shift+M) mostra os personagens como avatares num quadro. Arraste de um personagem até outro para ligá-los.

- Cada ligação tem um **tipo** (Família, Amor, Amizade, Conflito, Trabalho, Outro), com cor própria, e um **rótulo** opcional, como "irmãs" ou "ex-marido".
- **Mostrar quem divide cenas** liga quem aparece junto, **Arrumar em círculo** organiza o quadro e **Caber na janela** mostra todos.
- As relações ficam salvas no `.fountain` e podem ser desfeitas.

## Visual e temas

- Tema **claro** e **escuro**, seguindo o sistema por padrão (Exibir → Tema, Configurações, ou o botão no canto da barra de menus).
- **Configurações** em páginas (Geral, Aparência, Roteiro, Salvamento): idioma da interface, reabrir o último roteiro ao iniciar, tema, **fonte e tamanho da interface**, convenções do roteiro, **fonte das páginas** (qualquer fonte monoespaçada instalada, sempre em 12 pt na grade de 10 caracteres por polegada, então o número de páginas não muda) e salvamento.
- Todo item de menu tem ícone; os que estão ligados (modo atual, elemento, modo foco) ficam na cor de destaque.
- **Barra lateral** com cenas e personagens; o botão à esquerda da barra de ferramentas (Ctrl+\\) esconde ou mostra os dois painéis.
- Janela de impressão e PDF com a prévia das folhas uma embaixo da outra.
- Ícones SVG próprios (`resources/icons`), recoloridos automaticamente para cada tema.
- Painéis em cartões: cenas com número, página e quem fala, e personagens com avatar.
- **Modo foco** (Ctrl+Shift+F): esmaece tudo menos o trecho atual. Numa fala, o personagem, os parentéticos e o diálogo ficam em destaque juntos.

## Beat Board

Um **quadro livre para ideias** (Exibir → Beat Board, Ctrl+Shift+B):

- **Beats** são cartões com título, descrição, cor e uma **imagem** opcional. Arraste para mover, puxe o canto para redimensionar, duplo clique para editar. Duplo clique no quadro vazio cria um beat.
- **Imagens**: solte arquivos de imagem no quadro, cole uma imagem com Ctrl+V ou escolha na janela do beat. Elas são reduzidas e guardadas em JPEG dentro do próprio roteiro.
- **Linhas de fluxo** ligam um beat a outro: arraste o + da borda de um beat até o próximo. Clique numa linha e aperte Delete para apagá-la.
- Espaço ou o botão do meio do mouse arrastam o quadro; Ctrl+roda dá zoom; "Enquadrar" mostra todos os beats. Tudo pode ser desfeito, e o quadro fica salvo dentro do `.fountain`, num bloco de comentário que outros apps Fountain ignoram.

## Fichas dos personagens

Clique num personagem no painel lateral para abrir a ficha dele:

- **Foto, nome, papel** (protagonista, antagonista, coadjuvante…), **idade, ocupação, características** e **sinopse**.
- Quantas falas, cenas e palavras ele tem, onde aparece pela primeira vez e a lista das cenas em que fala. Duplo clique numa cena leva até ela.
- **Trocar o nome** na ficha renomeia todas as deixas do roteiro, mantendo extensões como (V.O.). Um Ctrl+Z desfaz.
- **Janela de personagens** (Documento → Personagens): uma tabela editável com todo o elenco, com papel, idade, ocupação, falas, cenas e palavras. Dá para buscar, criar personagens, abrir a ficha, criar avatar, enviar foto ou ir para a primeira fala. Renomear ali atualiza todas as deixas, a ficha e o mapa de relações, e juntar dois nomes pede confirmação.
- **Criador de avatar 16-bit**: monte um retrato em pixel art numa grade de 32×32 com cores sombreadas. Escolha o tipo de corpo e depois pele, olhos, sobrancelhas, boca, cabelo, barba, óculos, chapéu, roupa e fundo, cada um com opções de cor. Tem desfazer e refazer, botão Aleatório e Redefinir.
- O painel Personagens tem dois botões no cabeçalho: um abre a janela de personagens e o outro esconde o painel (Exibir → Personagens traz de volta).
- A foto aparece no painel e nos cartões. Fichas e fotos ficam guardadas dentro do próprio `.fountain`, num bloco de comentário que outros apps Fountain ignoram.

O estilo fica em [`resources/themes/estilo.qss`](resources/themes/estilo.qss). É CSS do Qt com variáveis de cor (`@accent`, `@surface`, …), definidas em `src/theme.cpp`. Para editar e ver o resultado ao salvar, sem recompilar:

```bash
OWLIO_QSS=resources/themes/estilo.qss ./run.sh
```

Para forçar um tema numa execução: `./run.sh --theme dark`.

## Baixar

Cada [versão](https://github.com/berdfandrade/owlio/releases) traz um instalador para Windows (`Owlio-Setup-<versão>.exe`). Ele instala para o usuário atual sem pedir administrador, cria atalho no menu Iniciar (e na área de trabalho, se você quiser) e pode abrir arquivos `.fountain` e `.fdx` com o Owlio. Os instaladores estão passando a ser assinados de graça pela SignPath Foundation (veja a [política de assinatura](docs/CODE_SIGNING.md)). Até sair uma versão assinada, o SmartScreen do Windows pode avisar de editor desconhecido: clique em **Mais informações → Executar assim mesmo**.

## Compilar

Requer CMake ≥ 3.16, g++ com C++17 e Qt 6 (Widgets, PrintSupport, Svg; Test para o teste de interface).

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=$HOME/Qt/6.8.3/gcc_64
cmake --build build -j
```

Com o Qt instalado pelo sistema (`sudo apt install qt6-base-dev libxcb-cursor0`), não é preciso passar `CMAKE_PREFIX_PATH`.

## Executar

```bash
./run.sh examples/a_ultima_sessao.fountain   # ou examples/the_last_show.fountain, em inglês
```

O `run.sh` aponta para o Qt baixado em `~/Qt` e para a `libxcb-cursor0` extraída localmente.

Linha de comando:

```bash
./run.sh roteiro.fountain --export-pdf roteiro.pdf
```

## Testes

```bash
./build/owlio_tests          # núcleo: Fountain, FDX, paginação, edição, fichas, diálogo duplo, beat board
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
| `src/characterdialog.*` | Janela da ficha do personagem |
| `src/castdialog.*` | Janela de personagens (tabela editável do elenco) |
| `src/pixelavatar.*` | Criador de avatar 16-bit |
| `src/autosave.*` | Salvamento automático, cópias de recuperação e backups |
| `src/versionbrowser.*` | Janela de versões: versões salvas empilhadas em perspectiva, com linha do tempo |
| `src/settingsdialog.*`, `src/i18n.*` | Configurações, idioma da interface e carregamento das traduções |
| `src/avatars.*` | Fotos dos personagens: importação, recorte, cache e desenho em círculo |
| `src/theme.*` | Paleta, QSS com variáveis, ícones SVG recoloridos, claro/escuro |
| `src/carddelegate.*` | Cartões dos painéis de cenas e personagens |
| `src/sceneboard.*` | Tela de cartões (grade ou lista, com arrastar e soltar) |
| `src/timeline.*` | Linha do tempo e a estrutura da história editável |
| `src/relationmap.*` | Mapa de relações |
| `src/beatboard.*`, `src/beatdialog.*` | Beat Board (quadro em QGraphicsView, beats, linhas de fluxo) e a janela do beat |
| `packaging/`, `.github/workflows/windows.yml` | Instalador do Windows (Inno Setup) e o workflow que o gera e publica as versões |
| `translations/` | Traduções do Qt: `owlio_pt_BR.ts` (português) e `owlio_en.ts` (plurais em inglês) |

O núcleo (`document`, `layout`, `editor`, `fountain`, `fdx`) não depende do Qt.

## Traduções

Todos os textos do código estão em inglês; o português vem de `translations/owlio_pt_BR.ts`. Depois de mudar textos da interface, atualize o arquivo e traduza as entradas novas no Qt Linguist:

```bash
cmake --build build --target update_translations
~/Qt/6.8.3/gcc_64/bin/linguist translations/owlio_pt_BR.ts
```

## Licença

O Owlio é software livre sob a [GNU General Public License v3.0](LICENSE).

Assinatura de código gratuita fornecida pela [SignPath.io](https://signpath.io), certificado da [SignPath Foundation](https://signpath.org).
