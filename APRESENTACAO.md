Alunos: LINYKER VINICIUS GOMES BARBOSA - 556280, VITOR LOULA SILVA - 540622

# Apresentação: Sockets e Streams em C++ (Trabalho-1)

Este documento apresenta a fundamentação teórica e a implementação prática dos conceitos de Sockets, Streams, Serialização e Multicast, baseando-se no projeto **Sincronização Distribuída de Arquivos** localizado em `Trabalho-1`.

---

## 1. Definição do Serviço Remoto

**Serviço:** `Sistema de Sincronização Distribuída de Arquivos`

O serviço permite que clientes enviem metadados de arquivos para um servidor Líder (Leader), que então sincroniza essas informações com servidores de Backup via Multicast.

### Classes de Dados (Equivalente a POJO)

#### Classe 1: `File`

Representa os metadados de um arquivo no sistema.

```cpp
class File {
    uint64_t _id;
    uint64_t _folder_id;
    std::string _name;
    uint64_t _size_bytes;
    std::vector<char> _content;
    // Getters e Setters...
};
```

#### Classe 2: `Folder`

Representa uma pasta que agrupa arquivos.

```cpp
class Folder {
    uint64_t _id;
    std::string _name;
    // Getters e Setters...
};
```

### Classes de Modelo de Serviço

1.  **`RunAsLeader`**: Implementa o serviço principal que aceita conexões TCP de clientes e processa requisições.
2.  **`RunAsBackup`**: Implementa o serviço de redundância que ouve atualizações via Multicast para manter a consistência.

---

## 2. Implementação de Custom Streams (Output)

No C++, utilizamos a biblioteca padrão `<ostream>` como base para fluxos de saída.

### Classe `FileOutputStream`

Responsável por enviar um conjunto de objetos `File` através de um stream de destino.

**Regras atendidas:**

- (i) Recebe um array de ponteiros/objetos `File`.
- (ii) Recebe a quantidade de objetos (`count`).
- (iii) Envia metadados (como nome e IDs) que ocupam bytes específicos no fluxo.
- (iv) Recebe um `std::ostream&` como destino (Polimorfismo para Console, Arquivo ou Socket).

```cpp
class FileOutputStream {
public:
    FileOutputStream(const File* files, std::size_t count, std::ostream& destination);

    void write() {
        for (std::size_t i = 0; i < _count; ++i) {
            // (iii) Envia metadados nativos e conteúdo usando métodos robustos de abstração
            WriteUint64(_destination, _files[i].getId());
            WriteUint64(_destination, _files[i].getFolderId());
            WriteString(_destination, _files[i].getName());
            WriteUint64(_destination, _files[i].getSizeBytes());

            const auto& content = _files[i].getContent();
            if (!content.empty()) {
                _destination.write(content.data(), static_cast<std::streamsize>(content.size()));
            }
        }
    }
};
```

### Testes de Destino

- **Saída Padrão**: `FileOutputStream(files, n, std::cout).write();`
- **Arquivo**: `std::ofstream file("saida.dat"); FileOutputStream(files, n, file).write();`
- **TCP**: Utiliza-se um `std::stringstream` para serializar e depois `SocketUtils::SendAll` para enviar pelo socket TCP.

---

## 3. Implementação de Custom Streams (Input)

### Classe `FileInputStream`

Lê os dados gerados pelo processo anterior a partir de um `std::istream`.

```cpp
class FileInputStream {
public:
    explicit FileInputStream(std::istream& source);

    std::vector<File> readFiles(int count) {
        std::vector<File> files;
        files.reserve(static_cast<std::size_t>(count));

        for (int i = 0; i < count; ++i) {
            const std::uint64_t id = ReadUint64(_source);
            const std::uint64_t folder_id = ReadUint64(_source);
            const std::string name = ReadString(_source);
            const std::uint64_t size_bytes = ReadUint64(_source);

            std::vector<char> content;
            if (size_bytes > 0) {
                content.resize(static_cast<std::size_t>(size_bytes));
                _source.read(content.data(), static_cast<std::streamsize>(size_bytes));
            }

            files.emplace_back(id, folder_id, name, size_bytes, std::move(content));
        }
        return files;
    }
};
```

### Testes de Origem

- **Entrada Padrão**: `std::cin`
- **Arquivo**: `std::ifstream input("dados.dat");`
- **TCP**: Recebe o buffer do socket e inicializa um `std::stringstream` para leitura.

---

## 4. Serialização: Pack e Unpack

A serialização no projeto é feita de forma manual para garantir eficiência e controle total sobre o layout dos bytes.

- **Empacotamento (Pack)**: O cliente utiliza `SocketUtils::SendUint32` para metadados e `SocketUtils::SendAll` para o payload binário.
- **Desempacotamento (Unpack)**: O servidor utiliza `SocketUtils::ReceiveUint32` e `SocketUtils::RecvAll`, reconstruindo os objetos `File` via `FileInputStream`.

---

## 5. Multicast e Alta Disponibilidade

O projeto utiliza Multicast IP para manutenção do cluster de servidores.

### Funcionalidades:

1.  **Heartbeat**: O Líder envia mensagens periódicas (`HEARTBEAT|LIDER`) para o grupo `239.0.0.1`.
2.  **Sincronização (Sync)**: Quando o Líder recebe novos arquivos, ele propaga os dados para os Backups via Multicast (`SYNC|ID,Nome`).

```cpp
// Exemplo de envio Multicast (src/Utils.cpp)
SocketUtils::SendUdpMulticast(
    "239.0.0.1",
    9000,
    "SYNC|" + dados_arquivos
);
```

### Tolerância a Falhas:

Se um servidor Backup para de receber o Heartbeat por 3 segundos, ele assume que o Líder falhou e tenta realizar o Bind na porta principal para se tornar o novo Líder.
