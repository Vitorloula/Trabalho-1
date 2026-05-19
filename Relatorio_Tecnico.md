# Relatório de Avaliação Técnica: Implementação de RMI (Remote Method Invocation) em C++

**Disciplina:** Sistemas Distribuídos  
**Trabalho 2:** Remote Method Invocation (RMI)
**Alunos:** Linyker Vinicius Gomes Barbosa (556280), Vitor Loula Silva (540622)

---

## 1. Resumo do Projeto

O presente projeto expande o contexto do sistema de Sincronização Distribuída de Arquivos (desenvolvido no Trabalho 1), substituindo a comunicação manual via sockets por uma arquitetura robusta de **Invocação Remota de Método (RMI)**. O sistema foi integralmente remodelado para adotar um protocolo estrito de **requisição-resposta** (Request-Reply Protocol). 

A aplicação foi desenvolvida em **C++17**, garantindo que as regras de negócio sejam isoladas dos detalhes de transporte de rede, encapsulados em um módulo de IPC (Inter-Process Communication).

---

## 2. Requisitos Estruturais Atendidos

O modelo de domínio e os componentes RMI foram rigorosamente estruturados para preencher as exigências estipuladas.

### 2.1 Entidades (Mínimo de 4 classes)
O domínio de dados possui 5 entidades principais:
1. **`StorageNode`**: Classe base abstrata para nós de armazenamento.
2. **`File`**: Representa um arquivo físico e seus metadados.
3. **`Folder`**: Representa uma pasta/diretório de organização.
4. **`User`**: Representa os dados do usuário.
5. **`Workspace`**: Representa a área de trabalho do usuário que gerencia múltiplos arquivos.

### 2.2 Composições do Tipo Extensão - "É-um" (Mínimo de 2)
Implementado via herança base em C++:
1. `File` **estende** (`public`) `StorageNode`.
2. `Folder` **estende** (`public`) `StorageNode`.

### 2.3 Composições do Tipo Agregação - "Tem-um" (Mínimo de 2)
A classe `Workspace` compõe estruturalmente outras entidades:
1. `Workspace` **tem-um** `User` (representando o `_owner` - dono da área de trabalho).
2. `Workspace` **tem-uma** lista de `File` (`std::vector<File> _files`).

---

## 3. Arquitetura do Cliente (Proxy)

A camada de cliente atua solicitando serviços remotamente de forma transparente. O transporte (TCP) foi abstraído pelo padrão **Proxy** através da classe `IPCModule`, encapsulando a serialização em formato **JSON** (via `nlohmann::json`) como Representação Externa de Dados.

### 3.1 Funções Core do Cliente
- **`doOperation(RemoteObjectRef o, std::string methodId, std::string arguments)`**: Único ponto de contato da aplicação cliente com a rede. Serializa uma mensagem de requisição, envia ao servidor, bloqueia a _thread_ aguardando, e converte o payload JSON retornado pela resposta.

### 3.2 Passagem por Valor e Referência (Visão do Cliente)
- **Passagem por Valor**: Ao realizar upload de arquivos (`uploadFile`), o cliente envia uma cópia integral (parâmetros e dados binários) do objeto `File` no payload JSON.
- **Passagem por Referência**: Ao solicitar o contexto da área de trabalho, o cliente não recebe o objeto `Workspace` pesando na rede. Em vez disso, ele recebe uma estrutura `RemoteObjectRef` que aponta para a entidade no servidor (IP, porta e `objectId`).

---

## 4. Arquitetura do Servidor (Dispatcher)

O servidor RMI atua no padrão **Dispatcher**, operando o laço de recebimento de requisições, interpretação (unmarshalling) e despacho para o método de negócio correto (em `Server.cpp`).

### 4.1 Funções Core do Servidor
No lado do servidor, a classe `IPCModule` expõe os métodos exigidos (adaptados para a API de Sockets C++):
- **`getRequest()`**: Ouve as conexões ativas na porta vinculada e extrai integralmente o pacote JSON da requisição, retornando também o _Socket_ cliente.
- **`sendReply(SocketType client_fd, const std::string& replyJson)`**: Transforma os dados de saída, formata o pacote de sucesso/erro e devolve a resposta final ao cliente, encerrando a conexão correspondente.

### 4.2 Métodos Remotos de Negócio (Mínimo de 4)
O servidor RMI mapeia **quatro (4)** procedimentos expostos. A partir do `methodId` contido no pacote, o servidor resolve a execução para:

1. **`uploadFile`**: Instancia um objeto abstrato `File` a partir dos argumentos (recebidos por **valor** via JSON) e grava seu conteúdo físico no disco da máquina servidora.
2. **`listFiles`**: Recebe a identificação de um _Workspace_ específico e extrai a relação de arquivos vinculados a ele para retornar ao solicitante.
3. **`deleteFile`**: Comando para destruir localmente no servidor o binário e a referência que corresponde ao arquivo indicado.
4. **`getWorkspaceRef`**: Retorna um contexto abstrato. Demonstra fisicamente a **passagem por referência** construindo e repassando o ponteiro de localização lógica do objeto no servidor (representado pela estrutura `RemoteObjectRef`).

---

## 5. Conclusão da Refatoração

Esta versão satisfaz completamente as exigências para o **Trabalho 2**. A lógica de sincronização (incluindo o _Heartbeat_ e _failover_) permaneceu íntegra, porém totalmente livre de lógica direta de Socket nas operações da aplicação. A divisão Proxy e Dispatcher viabilizou uma abstração de Invocação de Método Remoto polida e aderente aos padrões literários do ramo.
