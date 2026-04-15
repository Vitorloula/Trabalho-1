# Relatório de Avaliação Técnica: Sockets e Streams em C++

**Disciplina:** Sistemas Distribuídos  
**Trabalho 1**
**Alunos:** Linyker Vinicius Gomes Barbosa (556280), Vitor Loula Silva (540622)

---

## 1. Resumo do Projeto

O projeto implementa um sistema de **Sincronização Distribuída de Arquivos** fundamentado na arquitetura Líder-Backup. O sistema foca na robustez do transporte de dados via TCP para comunicação cliente-servidor e na eficiência do protocolo UDP Multicast para replicação de estado e _heartbeating_ entre servidores.

O fluxo básico consiste em um Cliente selecionando um arquivo, que é serializado e enviado ao Líder. O Líder processa a requisição, salva o arquivo localmente e propaga a notificação de sincronização para os Backups via Multicast.

---

## 2. Modelagem de Dados e Protocolo

A base da comunicação reside na serialização de objetos de domínio. Foram implementadas classes de dados que garantem a integridade da informação em trânsito.

### Entidades Core

- **`File`**: Encapsula metadados (ID, FolderID, Nome, Tamanho) e o payload binário (`std::vector<char>`).
- **`Folder`**: Define o agrupamento lógico para a organização dos arquivos (ID, ParentID, Nome).

---

## 3. Arquitetura de Streams Customizados

Para atender aos requisitos de abstração de E/S, o projeto utiliza o polimorfismo de streams do C++, permitindo que a lógica de negócio ignore a origem ou destino físico dos dados através das classes base `std::ostream` e `std::istream`.

### 3.1 Camada de Saída (`FileOutputStream`)

A classe `FileOutputStream` implementa a serialização personalizada. Ao receber um `std::ostream&`, ela injeta os dados de um ou mais objetos `File` no fluxo:

- **Metadados:** IDs e tamanhos são gravados como tipos de largura fixa.
- **Flexibilidade:** Pode ser direcionada para `std::cout` (depuração), `std::ofstream` (persistência em disco) ou `std::stringstream` (bufferização para rede).

### 3.2 Camada de Entrada (`FileInputStream`)

Inversamente, a `FileInputStream` consome um `std::istream&`. A leitura é estruturada para reconstruir os objetos de forma segura:

- **Recuperação de Tipos:** Lê metadados na ordem inversa da escrita.
- **Segurança de Memória:** O conteúdo do arquivo é alocado dinamicamente via `std::vector` somente após a leitura do metadado de tamanho, prevenindo acessos inválidos.

---

## 4. Implementação de Sockets e Serialização

Diferente de bibliotecas de terceiros (como Protocol Buffers ou JSON), optou-se pela serialização manual para demonstrar o controle sobre o layout de memória:

### Serialização Manual (Pack/Unpack)

1.  **Metadados de Controle:** Antes do payload, o sistema envia a quantidade de arquivos e o tamanho total do pacote como `uint32_t` (Big-Endian compatível via headers de rede).
2.  **Payload Binário:** Os dados dos objetos `File` são transmitidos de forma contígua para maximizar o _throughput_ do socket.

### Comunicação TCP

O servidor opera em modo `RunAsLeader` gerenciando o ciclo de vida das conexões:

- **Handshake/Protocolo:** O cliente envia o `count` e o `payload_size`, seguidos pelos dados reais.
- **Resposta:** O servidor confirma o recebimento com uma mensagem textual ("Arquivos recebidos com sucesso!").

---

## 5. Multicast e Alta Disponibilidade

O diferencial técnico do projeto é a implementação da camada de coordenação via UDP Multicast (endereço `239.0.0.1`, porta `9000`) para garantir a sobrevivência do serviço.

### Mecanismos de Consistência

- **Heartbeat Progressivo:** O Líder emite sinais vitais a cada 1 segundo. Se um Backup detectar a ausência de sinal por mais de **3 segundos**, ele inicia o procedimento de _failover_.
- **Eleição de Líder:** O primeiro Backup que conseguir realizar o `bind()` na porta TCP `8080` após a falha do Líder assume o papel de novo coordenador.
- **Replicação de Estado (Sync):** Toda alteração confirmada no Líder gera uma mensagem `SYNC|ID,Nome` via Multicast, permitindo que os Backups mantenham seus índices de arquivos atualizados.
