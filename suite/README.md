# Media Suite

Aplicativo Windows que reúne YT Downloader, LocalFlix e MagnifyFactory em uma
única janela Qt. Biblioteca e player mantêm o tema do LocalFlix; downloads,
conversão e edição usam a mesma paleta, barra lateral e controles.

- **Biblioteca:** séries, filmes, coleções, ordem manual, progresso, reprodução
  automática, capítulos, pular abertura/encerramento e picture in picture.
- **Baixar vídeos:** links do YouTube, Google Drive e demais sites compatíveis
  com yt-dlp, arquivos de mídia diretos, vídeos incorporados em páginas e streams
  HLS/DASH sem DRM. Prévia, vídeo no melhor formato disponível ou MP3, qualidades,
  playlists/pastas, até três downloads simultâneos, cancelamento e nova tentativa.
  A disponibilidade depende do site, das permissões e do suporte do extrator.
- **Integração:** vídeos baixados podem entrar automaticamente na biblioteca;
  cada download oferece acesso ao conversor. Na fila do conversor, o menu de
  um vídeo concluído permite adicioná-lo à biblioteca.
- **Converter e editar:** motores, predefinições, pastas monitoradas e editores
  de vídeo, áudio, imagem, PDF e modelos 3D do MagnifyFactory.

## Executar

Abra `MediaSuite.exe` na pasta distribuída. Não é necessário instalar Python
ou Qt. FFmpeg e FFprobe são incluídos quando disponíveis ao empacotar.
Node.js ou Deno no PATH habilitam os desafios JavaScript do YouTube.
Cole um link HTTP/HTTPS e pressione **Buscar**. A origem é detectada
automaticamente. Links de arquivos de vídeo do Drive (`file/d/…`, `open?id=…`
e `uc?id=…`) usam o extrator GoogleDrive; pastas compartilhadas usam o suporte
de playlists/pastas do yt-dlp. A prévia mostra a plataforma, duração e qualidades
quando essas informações estão disponíveis. Um link de playlist/pasta é
reconhecido automaticamente para baixar seus itens.

Para conteúdo com login, selecione um arquivo de cookies no formato
Netscape/Mozilla de uma conta com acesso, no campo opcional **Escolher cookies**.
O mesmo arquivo é usado na busca e no download. O aplicativo não extrai cookies
do navegador automaticamente. Arquivos privados exigem as permissões da conta;
DRM, sites sem extrator compatível e bloqueios do servidor podem impedir o download.
Consulte a [lista de fontes do yt-dlp](https://github.com/yt-dlp/yt-dlp/blob/master/supportedsites.md).
PDFs, documentos, arquivos compactados e modelos 3D usam as mesmas ferramentas
externas do MagnifyFactory (Poppler, QPDF, LibreOffice, 7-Zip e as ferramentas
de modelos 3D exigidas pelo formato). Os diálogos avançados herdados mantêm
alguns textos em inglês.

Os dados da nova biblioteca ficam em `%APPDATA%\MediaSuite`, separados da
biblioteca antiga do LocalFlix. É possível importar novamente as mesmas pastas:
os vídeos não são copiados, movidos nem apagados. As preferências de pasta do
YT Downloader são reaproveitadas. `LOCALFLIX_DATA_DIR` permite escolher uma
biblioteca existente; feche outros aplicativos que a utilizem antes de abrir.

## Compilar

Requisitos: Windows x64, MSVC C++20, Ninja, CMake e Qt 6.7 ou mais recente
(Widgets, Sql, Svg, OpenGLWidgets, Concurrent, Network e Test).

```powershell
git clone https://github.com/victorhmrod/MediaSuite.git
cd MediaSuite
# Baixa o libmpv e verifica seu SHA-256 (requer 7-Zip).
.\scripts\fetch-suite-deps.ps1
# O padrão do kit é D:\Qt\6.7.3\msvc2019_64; altere se necessário.
$env:LOCALFLIX_QT_DIR = 'D:\Qt\6.7.3\msvc2019_64'
.\scripts\build-suite.cmd
```

O CMake procura os headers e a DLL do libmpv em
`third_party/libmpv`. Para outra localização, configure
`-DLOCALFLIX_LIBMPV_DIR=C:/caminho/libmpv` no diretório `build/suite`.
Os fontes dos outros aplicativos estão incorporados em `suite/vendor`,
incluindo as alterações locais existentes no momento da integração.

Para executar uma compilação de desenvolvimento, deixe o `bin` do Qt no PATH
e instale `yt-dlp[default]` no Python. `MEDIA_SUITE_PYTHON` escolhe o executável
Python. A versão distribuída usa o worker empacotado em `worker/`.

```powershell
python -m pip install 'yt-dlp[default]' pyinstaller
.\scripts\package-suite.ps1
ctest --test-dir build/suite --output-on-failure
python -m unittest discover -s suite/tests -p test_worker.py -v
```

Saídas: `dist/MediaSuite-1.1.0/MediaSuite.exe` e `dist/MediaSuite-1.1.0-win64.zip`.
O teste `tst_suite` cobre busca e download HTTP pela interface, importação na biblioteca, conversão
MP4 → MKV e navegação. Os testes Python verificam MP4, MP3, metadados de link
direto, vídeo em página HTML, HLS, playlists, cookies e erros. `MEDIA_SUITE_TEST_WORKER` permite executar
esses testes contra o worker empacotado. Os testes de reprodução do LocalFlix
usam os clipes sintéticos de `MEDIA_SUITE_TESTDATA_DIR` (configurável no CMake).
`--downloads` e `--tools` abrem diretamente as respectivas áreas.
`--screenshot arquivo.png` grava uma captura e encerra o aplicativo.

Os aplicativos de origem e seus scripts de distribuição continuam disponíveis.
Esta integração se aplica às versões Windows; não inclui as versões Android.

## Licenças

O aplicativo combinado é distribuído sob AGPL-3.0, com o código do LocalFlix
e do MagnifyFactory e seus avisos de licença preservados em `vendor/`.
Consulte também `THIRD_PARTY_NOTICES.md`.
