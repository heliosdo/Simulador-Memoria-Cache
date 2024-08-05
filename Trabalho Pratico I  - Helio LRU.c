#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>

#define MAX_TAM_MP 256 // 256kb o tamanho maximo da MP
#define MAX_TAM_MC 32 // 32kb o tamanho maximo da MC
#define KBYTES 1024
#define TAM_CELULA 4 // Tamanho da palavra é de 4 bytes (32 bits)

// Estrutura para uma linha na cache
typedef struct {
    bool valido; // Indica se a linha é valida
    char *tag; // Tag associada à linha
    int linhaUso; // Indicador de uso para LRU
    int *bloco; // Bloco de dados associados à linha
} Linha;

// Estrutura para um conjunto na cache
typedef struct {
    Linha *linhas; // Conjunto de linhas na cache
} Conjunto;

// Estrutura para a memoria cache
typedef struct {
    int tamMcKb; // Tamanho da Cache em KB
    int bitsEndereco; // Bits necessarios para enderecar a MP
    int bitsByteBloco; // Bits necessarios para identificar um bloco
    int bitsByteConjunto; // Bits necessarios para identificar um conjunto
    int bitsByteTag; // Bits necessarios para identificar uma tag
    int linhasConjunto; // Numero de linhas por conjunto
    int linhasMC; // Numero total de linhas na MC
    int *enderecos; // Enderecos acessados sendo ponteiro para alocacao dinamica
    Conjunto *conjuntos; // Conjuntos na cache (alocacao dinamica)
    int numEnderecoLeitura; // Contador de enderecos lidos
    int numAcertos; // Contador de acertos na cache
    int numFaltas; // Contador de falhas na cache
    int celulasBlocoMP; // Numero de células por bloco na cache
    int maxEnderecos; // Numero maximo de enderecos suportados
} MemoriaCache;

// Estrutura para a memoria principal (MP)
typedef struct {
    int tamMpKb; // Tamanho da MP em KB
    int celulasMP; // Numero total de células na MP
    int celulasBlocoMP; // Numero de células por bloco na MP
    int maxEnderecos; // Numero maximo de enderecos na MP
} MemoriaPrincipal;

//Converte um numero inteiro para uma string binaria de um comprimento especificado
void ConverteBin(int num, char *bin, int bits) {
    bin[bits] = '\0';
    for (int i = bits - 1; i >= 0; i--) {
        bin[i] = (num % 2) + '0';
        num /= 2;
    }
}

//Converte uma string binaria para um numero decimal
int ConverteDec(const char *bin) {
    int dec = 0;
    while (*bin) {
        dec = (dec << 1) + (*bin++ - '0');
    }
    return dec;
}

//Verifica se um numero é potência de 2, usado para validar entrada de dados
int validaBase2(int x) {
    int base2;
    double log2Result = log2(x);
    base2 = (log2Result == (int)log2Result);
    return base2;
}

//Calcula e atualiza os bits necessarios para o enderecamento na cache
void atualizarBits(MemoriaCache *cache, MemoriaPrincipal *mp) {
    cache->bitsEndereco = (int)log2(mp->celulasMP);
    cache->bitsByteBloco = (int)log2(mp->celulasBlocoMP);
}

//Atualiza os bits necessarios para o mapeamento de conjuntos e tags na cache
void atualizarmapeamentoLRU(MemoriaCache *cache) {
    cache->bitsByteConjunto = (int)log2(cache->linhasMC / cache->linhasConjunto);
    cache->bitsByteTag = cache->bitsEndereco - cache->bitsByteBloco - cache->bitsByteConjunto;
}

//Inicializa a cache, alocando memoria e configurando suas estruturas
void inicializarCache(MemoriaCache *cache) {
    cache->numEnderecoLeitura = 0;
    cache->numAcertos = 0;
    cache->numFaltas = 0;

    cache->enderecos = malloc(sizeof(int) * cache->maxEnderecos);
    cache->conjuntos = malloc(sizeof(Conjunto) * (cache->linhasMC / cache->linhasConjunto));

    for (int i = 0; i < (cache->linhasMC / cache->linhasConjunto); i++) {
        cache->conjuntos[i].linhas = malloc(sizeof(Linha) * cache->linhasConjunto);
        for (int j = 0; j < cache->linhasConjunto; j++) {
            cache->conjuntos[i].linhas[j].valido = false;
            cache->conjuntos[i].linhas[j].linhaUso = 0;
            cache->conjuntos[i].linhas[j].bloco = malloc(sizeof(int) * cache->celulasBlocoMP);
            cache->conjuntos[i].linhas[j].tag = malloc(sizeof(char) * (cache->bitsByteTag + 1));
            memset(cache->conjuntos[i].linhas[j].bloco, -1, sizeof(int) * cache->celulasBlocoMP);
        }
    }
}

//Atualiza os enderecos de um bloco de dados em uma linha de cache
void atualizarBloco(Linha *linha, int enderecoInicio, int celulasBloco) {
    for (int i = 0; i < celulasBloco; i++) {
        linha->bloco[i] = enderecoInicio + i;
    }
}


// Imprime o estado atual da cache
void imprimirEstadoCache(MemoriaCache *cache, int conjuntoIndex, int linhaIndex) {
    printf("\nEstado da Cache (Conjunto %d):\n", conjuntoIndex);
    printf("------------------------------\n");
    for (int i = 0; i < cache->linhasConjunto; i++) {
        if (i == linhaIndex) {
            printf("-> "); // Indica a linha acessada
        } else {
            printf("   ");
        }
        printf("Linha %2d: ", i);
        if (cache->conjuntos[conjuntoIndex].linhas[i].valido) {
            printf("[V] Tag: %s, Bloco: [", cache->conjuntos[conjuntoIndex].linhas[i].tag);
            for (int j = 0; j < cache->celulasBlocoMP; j++) {
                if (j > 0) printf(", ");
                printf("%d", cache->conjuntos[conjuntoIndex].linhas[i].bloco[j]);
            }
            printf("] Uso: %d\n", cache->conjuntos[conjuntoIndex].linhas[i].linhaUso);
        } else {
            printf("[ ]\n");
        }
    }
    printf("------------------------------\n");
}

//Implementa o algoritmo LRU para gerenciar a cache, verificando hits e misses, e atualizando a cache com novas entradas
void mapeamentoLRU(MemoriaCache *cache) {
    int endereco = cache->enderecos[cache->numEnderecoLeitura];
    char binEndereco[32];
    ConverteBin(endereco, binEndereco, cache->bitsEndereco);

    char *tag = malloc(sizeof(char) * (cache->bitsByteTag + 1));
    char *indexConjunto = malloc(sizeof(char) * (cache->bitsByteConjunto + 1));

    strncpy(tag, binEndereco, cache->bitsByteTag);
    tag[cache->bitsByteTag] = '\0';

    strncpy(indexConjunto, binEndereco + cache->bitsByteTag, cache->bitsByteConjunto);
    indexConjunto[cache->bitsByteConjunto] = '\0';

    int conjuntoIndex = ConverteDec(indexConjunto);
    Conjunto *conjunto = &cache->conjuntos[conjuntoIndex];

    bool hit = false;
    int linhaIndex = -1;

    printf("======================================================================================\n");

    imprimirEstadoCache(cache, conjuntoIndex, -1);

    for (int i = 0; i < cache->linhasConjunto; i++) {
        Linha *linha = &conjunto->linhas[i];
        if (linha->valido && strcmp(linha->tag, tag) == 0) {
            hit = true;
            linhaIndex = i;
            linha->linhaUso = 0;
            break;
        }
    }

    if (hit) {
        printf("Cache HIT: Endereco %d encontrado no conjunto %d, linha %d.\n", endereco, conjuntoIndex, linhaIndex);
        cache->numAcertos++;
        printf("======================================================================================\n");
    } else {
        printf("Cache MISS: Endereco %d nao encontrado no conjunto %d. Buscando linha para substituir...\n", endereco, conjuntoIndex);
        cache->numFaltas++;

        int lruIndex = 0;
        int maxUso = conjunto->linhas[0].linhaUso;
        for (int i = 1; i < cache->linhasConjunto; i++) {
            if (conjunto->linhas[i].linhaUso > maxUso) {
                maxUso = conjunto->linhas[i].linhaUso;
                lruIndex = i;
            }
        }

        Linha *linhaLRU = &conjunto->linhas[lruIndex];
        linhaLRU->valido = true;
        strncpy(linhaLRU->tag, tag, cache->bitsByteTag);
        linhaLRU->tag[cache->bitsByteTag] = '\0';
        linhaLRU->linhaUso = 0;

        int enderecoBlocoInicio = endereco - (endereco % cache->celulasBlocoMP);
        atualizarBloco(linhaLRU, enderecoBlocoInicio, cache->celulasBlocoMP);

        printf("Substituindo linha %d no conjunto %d com tag %s.\n", lruIndex, conjuntoIndex, linhaLRU->tag);
        linhaIndex = lruIndex;

        imprimirEstadoCache(cache, conjuntoIndex, linhaIndex); //Só vamos realizar a impressão do novo estado do conjunto se der MISS, em caso de hit utiliza a impressao ja realizada anteriormente

        printf("======================================================================================\n");
    }

    for (int i = 0; i < cache->linhasConjunto; i++) {
        if (i != linhaIndex) {
            conjunto->linhas[i].linhaUso++;
        }
    }

    cache->numEnderecoLeitura++;

    free(tag);
    free(indexConjunto);
}

//Configura a memoria principal (MP) e a cache (MC) com parâmetros fornecidos pelo usuario atraves do terminal
void defineConfigTerminal(MemoriaCache *cache, MemoriaPrincipal *mp) {
    do {
        printf("Digite o tamanho da MP (em KBs, Max 256KB): ");
        scanf("%d", &mp->tamMpKb);
    } while (mp->tamMpKb < 1 || mp->tamMpKb > MAX_TAM_MP);

    do {
        printf("Quantidade de células por bloco na MP (2, 4 ou 8): ");
        scanf("%d", &mp->celulasBlocoMP);
    } while (!(mp->celulasBlocoMP == 2 || mp->celulasBlocoMP == 4 || mp->celulasBlocoMP == 8));

    mp->celulasMP = (mp->tamMpKb * KBYTES) / TAM_CELULA;
    cache->celulasBlocoMP = mp->celulasBlocoMP;
    mp->maxEnderecos = mp->celulasMP;

    do {
        printf("Digite o tamanho da memoria cache (em KBs, Max 32KB): ");
        scanf("%d", &cache->tamMcKb);
    } while (cache->tamMcKb < 1 || cache->tamMcKb > MAX_TAM_MC);

    cache->linhasMC = (cache->tamMcKb * KBYTES) / (mp->celulasBlocoMP * TAM_CELULA);

    do {
        printf("Digite o numero de linhas, na base 2, por conjunto da cache (minimo de 2 e maximo %d): ", cache->linhasMC / 2);
        scanf("%d", &cache->linhasConjunto);
    } while (cache->linhasConjunto > (cache->linhasMC / 2) || cache->linhasConjunto < 2 || !validaBase2(cache->linhasConjunto));
}


//Configura a memoria principal (MP) e a cache (MC) com parâmetros fornecidos pelo usuario atraves do do arquivo config.txt (os if sao validacoes)
int defineConfigArquivo(MemoriaCache *cache, MemoriaPrincipal *mp) {
    char filename[100];
    printf("\n\nRegras: Primeira Linha contem tamanho da MP em KBs seguido do tamanho do Bloco");
    printf("\nSegunda Linha contem tamanho da MC em KBs seguido de quantas linhas por conjunto");
    printf("\nExemplo \n256 8\n1 4");
    printf("\nDigite o nome do arquivo de configuracoes: ");
    scanf("%s", filename);
    FILE *file = fopen(filename, "r");


    if (file == NULL) {
        perror("Erro ao abrir o arquivo de configuracao");
        return -1;
    }

    if (fscanf(file, "%d %d", &mp->tamMpKb, &mp->celulasBlocoMP) != 2) {
        fprintf(stderr, "Erro: Formato invalido na configuracao da MP.\n");
        fclose(file);
        return -1;
    }
    if (mp->tamMpKb < 1 || mp->tamMpKb > MAX_TAM_MP) {
        fprintf(stderr, "Erro: Tamanho da MP invalido no arquivo de configuracao\n");
        fclose(file);
        return -1;
    }
    if (!(mp->celulasBlocoMP == 2 || mp->celulasBlocoMP == 4 || mp->celulasBlocoMP == 8)) {
        fprintf(stderr, "Erro: Quantidade de células por bloco na MP invalida no arquivo de configuracao\n");
        fclose(file);
        return -1;
    }
    mp->celulasMP = (mp->tamMpKb * KBYTES) / TAM_CELULA;
    cache->celulasBlocoMP = mp->celulasBlocoMP;
    mp->maxEnderecos = mp->celulasMP;

    if (fscanf(file, "%d %d", &cache->tamMcKb, &cache->linhasConjunto) != 2) {
        fprintf(stderr, "Erro: Formato invalido na configuracao da MC.\n");
        fclose(file);
        return -1;
    }
    if (cache->tamMcKb < 1 || cache->tamMcKb > MAX_TAM_MC) {
        fprintf(stderr, "Erro: Tamanho da MC invalido no arquivo de configuracao\n");
        fclose(file);
        return -1;
    }
    cache->linhasMC = (cache->tamMcKb * KBYTES) / (mp->celulasBlocoMP * TAM_CELULA);

    if (cache->linhasConjunto > (cache->linhasMC / 2) || cache->linhasConjunto < 2 || !validaBase2(cache->linhasConjunto)) {
        fprintf(stderr, "Erro: Numero de linhas por conjunto invalido no arquivo de configuracao\n");
        fclose(file);
        return -1;
    }

    fclose(file);
    return 0;
}

//Lê enderecos fornecidos pelo usuario e atualiza a cache até usuario digitar -1
void acessoMpTerminal(MemoriaCache *cache, MemoriaPrincipal *mp) {
    int endereco;
    int numEnderecos = cache->numEnderecoLeitura; //Poderiamos inicializar em 0, porém perderia a sequencia de acessos a cache, entao puxamos do numEnderecoLeitura;

    if(!configMcOk(cache)){
        perror("Configuracoes da MP e MC nao definidos!");
        return;
    }

    printf("Digite os enderecos (digite -1 para parar):\n");

    while (true) {
        scanf("%d", &endereco);
        if (endereco == -1) {
            break;
        }
        if (endereco < 0 || endereco >= mp->maxEnderecos) {
            printf("Erro: Endereco %d esta fora do intervalo esperado (0-%d).\n", endereco, mp->maxEnderecos - 1);
            continue;
        }

        cache->enderecos[numEnderecos] = endereco;
        numEnderecos++;
        mapeamentoLRU(cache);
    }
}

//Lê enderecos de um arquivo enderecos.txt e atualiza a cache
int acessoMpArquivo(MemoriaCache *cache, MemoriaPrincipal *mp) {
    char filename[100];
    printf("\n\nOBS: Os endereços devem estar no limite da MP e estarem separados por espacos");
    printf("\nDigite o nome do arquivo de enderecos: ");
    scanf("%s", filename);
    FILE *file = fopen(filename, "r");



    if(!configMcOk(cache)){
        perror("Configuracoes da MP e MC nao definidos!");
        return -1;
    }
    if (file == NULL) {
        perror("Erro ao abrir o arquivo de enderecos");
        return -1;
    }

    int endereco;
    int numEnderecos = cache->numEnderecoLeitura;//Poderiamos inicializar em 0, porém perderia a sequencia de acessos a cache, entao puxamos do numEnderecoLeitura;

    while (fscanf(file, "%d", &endereco) != EOF) {
        if (endereco < 0 || endereco >= mp->maxEnderecos) {
            printf("Erro: Endereco %d esta fora do intervalo esperado (0-%d).\n", endereco, mp->maxEnderecos - 1);
            fclose(file);
            return -1;
        }

        cache->enderecos[numEnderecos] = endereco;
        numEnderecos++;
        mapeamentoLRU(cache);
    }

    fclose(file);

    return 0;
}

//Verifica se ja foram inseridas as informacões das memorias MP e MC
int configMcOk(MemoriaCache *cache) {
    return (cache->tamMcKb > 0 && cache->linhasMC > 0 && cache->linhasConjunto > 0);

}

//Imprime as configuracões atuais da MP e MC
void imprimirConfigMpMc(MemoriaCache *cache, MemoriaPrincipal *mp) {
    printf("\n=== Configuracões da Memoria Principal (MP) ===\n");
    printf("\tTamanho da MP: %d KB\n", mp->tamMpKb);
    printf("\tCélulas por bloco na MP: %d\n", mp->celulasBlocoMP);
    printf("\tTotal de células na MP: %d\n", mp->celulasMP);
    printf("\tEnderecos maximos na MP: %d\n", mp->maxEnderecos);

    printf("\n=== Configuracões da Memoria Cache (MC) ===\n");
    printf("\tTamanho da MC: %d KB\n", cache->tamMcKb);
    printf("\tNumero de linhas na MC: %d\n", cache->linhasMC);
    printf("\tNumero de linhas por conjunto: %d\n", cache->linhasConjunto);
    printf("\tCélulas por bloco na MC: %d\n", cache->celulasBlocoMP);
    printf("\tBits para enderecamento: %d\n", cache->bitsEndereco);
    printf("\tBits para identificar o bloco: %d\n", cache->bitsByteBloco);
    printf("\tBits para identificar o conjunto: %d\n", cache->bitsByteConjunto);
    printf("\tBits para identificar a tag: %d\n", cache->bitsByteTag);
}


// Imprime as estatisticas de acertos e falhas da cache
void imprimirResultado(MemoriaCache *cache) {
    int totalAcessos = cache->numAcertos + cache->numFaltas;
    double porcentagemAcertos = (totalAcessos == 0) ? 0 : ((double)cache->numAcertos / totalAcessos) * 100;
    double porcentagemFaltas = (totalAcessos == 0) ? 0 : ((double)cache->numFaltas / totalAcessos) * 100;

    printf("\n=== Estatisticas da Memoria Cache ===\n");
    printf("Total de acertos: %d (%.2f%%)\n", cache->numAcertos, porcentagemAcertos);
    printf("Total de falhas: %d (%.2f%%)\n", cache->numFaltas, porcentagemFaltas);
}

// Imprime o conteudo da cache, incluindo cada conjunto e suas linhas.
void imprimirCache(MemoriaCache *cache) {

    if(!configMcOk(cache)){
        perror("Configuracoes da MP e MC nao definidos!");
        return;
    }

    printf("\n=== Conteudo da Memoria Cache ===\n");
    for (int i = 0; i < cache->linhasMC / cache->linhasConjunto; i++) {
        printf("Conjunto %d:\n", i);
        for (int j = 0; j < cache->linhasConjunto; j++) {
            Linha *linha = &cache->conjuntos[i].linhas[j];
            printf("  Linha %2d: ", j);
            if (linha->valido) {
                printf("[V] Tag: %s, Bloco: [", linha->tag);
                for (int k = 0; k < cache->celulasBlocoMP; k++) {
                    if (k > 0) printf(", ");
                    printf("%d", linha->bloco[k]);
                }
                printf("] Uso: %d\n", linha->linhaUso);
            } else {
                printf("[ ]\n");
            }
        }
    }
    printf("=================================\n");
}

int main() {
    MemoriaCache cache;
    MemoriaPrincipal mp;
    int menuOpcao;

    printf("Trabalho de Organizacao e Arquitetura de Computadores - Eng. Comp. - UFGD\n");
    printf("Prof.: Rodrigo Porfirio da Silva Sacchi\n");
    printf("Aluno: Helio Soares de Oliveira\n\n");
    printf("\t\tSimulador de Memoria Cache - LRU\n");

    do {
        printf("\nMenu Principal:\n");
        printf("\t1. Definir MP e Cache via Terminal\n");
        printf("\t2. Definir MP e Cache via arquivo 'config.txt' (Primeira linha Tamanho da MP(KBs) e Celulas por Bloco, Segunda Linha Tamanho da MC e Linhas por conjunto)\n");
        printf("\t3. Acessar endereco MP via terminal\n");
        printf("\t4. Acessar endereco MP via arquivo 'enderecos.txt' (Enderecos em decimal separados por espaco)\n");
        printf("\t5. Imprimir conteudo da cache\n");
        printf("\t6. Imprimir configuracao da MP e MC\n");
        printf("\t9. Sair\n");
        printf("Opcao: ");
        scanf("%d", &menuOpcao);

        switch (menuOpcao) {
            case 1:
                defineConfigTerminal(&cache, &mp);
                atualizarBits(&cache, &mp);
                atualizarmapeamentoLRU(&cache);
                inicializarCache(&cache);
                imprimirConfigMpMc(&cache, &mp);
                break;
            case 2:
                if (defineConfigArquivo(&cache, &mp) == 0) {
                    atualizarBits(&cache, &mp);
                    atualizarmapeamentoLRU(&cache);
                    inicializarCache(&cache);
                    imprimirConfigMpMc(&cache, &mp);
                } else {
                    printf("Erro na leitura do arquivo de configuracao. Voltando ao menu principal.\n");
                }
                break;
            case 3:
                acessoMpTerminal(&cache, &mp);
                imprimirResultado(&cache);
                break;
            case 4:
                if (acessoMpArquivo(&cache, &mp) == -1) {
                    printf("Erro na leitura do arquivo de enderecos. Voltando ao menu principal.\n");
                    break;
                }
                imprimirResultado(&cache);
                break;
            case 5:
                imprimirCache(&cache);
                break;
            case 6:
                imprimirConfigMpMc(&cache, &mp);
                break;
            case 9:
                printf("Encerrando o programa....\n");
                break;
            default:
                printf("Opcao Invalida.\n");
                break;
        }

    } while (menuOpcao != 9);

    printf("\nMemoria cache final:\n");
    imprimirCache(&cache);

    return 0;
}
