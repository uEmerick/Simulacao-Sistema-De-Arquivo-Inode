wae#include "Permissao.h"
#include "dividir.h"
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <ctype.h>

using namespace std;

#define TIPO_ARQUIVO_DIRETORIO 'd'
#define TIPO_ARQUIVO_ARQUIVO '-'
#define TIPO_ARQUIVO_LINK 'detalhado'

#define MAX_NOME_ARQUIVO 14
#define DIRETORIO_LIMITE_ARQUIVOS 10
#define MAX_INODEINDIRETO 5
#define ENDERECO_CABECA_LISTA 0
#define QUANTIDADE_LIMITE_ENDERECO_LISTA_BLOCO_LIVRE 10
#define PERMISSAO_PADRAO_DIRETORIO 755
#define PERMISSAO_PADRAO_ARQUIVO 644
#define PERMISSAO_PADRAO_LINKSIMBOLICO 777

enum cores 
{   
    DARKBLUE = 1, 
    DARKGREEN = 2,
    LIGHTBLUE = 3,
    RED = 4,
    PURPLE = 5,
    YELLOW = 6,
    WHITE = 7,
    GRAY = 8,
    BLUE = 9,
    GREEN = 10
};

void textcolor (int color)
{
    static int __BACKGROUND;

    HANDLE h = GetStdHandle ( STD_OUTPUT_HANDLE );
    CONSOLE_SCREEN_BUFFER_INFO csbiInfo;


    GetConsoleScreenBufferInfo(h, &csbiInfo);

    SetConsoleTextAttribute (GetStdHandle (STD_OUTPUT_HANDLE),
                            color + (__BACKGROUND << 4));
}


// INODE CONTROLA COMO ESTï¿½ï¿½ SENDO GRAVADO O ARQUIVO, EM QUAIS BLOCOS ESTï¿½ï¿½ GRAVADO O CONTEï¿½DO DO ARQUIVO, AS PERMISSï¿½ES, DATA
struct INode
{
    /*
        Campo de seguranca (estilo Linux):

        [0]  -> Tipo:
               'd' = diretorio
               '-' = arquivo regular
               'detalhado' = link

        [1-3] -> permissoes do dono (rwx)
        [4-6] -> permissoes do categoria (rwx)
        [7-9] -> permissoes de outros (rwx)

        [10]  -> '\0' (fim da string)
    */
    char seguranca[11];

    //numero de links fisicos apontando para este inode
    int quantidadeLinkFisico;

    //identificacao do dono e categoria
    int dono;
    int categoria;

    //datas (armazenadas como string)
    char momentoCriacao[20];
    char momentoUltimoAcesso[20];
    char momentoUltimaAlteracao[20];

    //tamanho do arquivo em bytes
    long long int dimensaoArquivo;

    /*
        ponteiros para blocos de dados:

        - enderecoDireto: atï¿½ 5 blocos diretos
        - enderecoSimplesIndireto: aponta para um bloco que contem outros ponteiros
        - enderecoDuploIndireto: ponteiro para bloco de ponteiros de ponteiros
        - enderecoTriploIndireto: nivel mais profundo de indirecao
    */
    int enderecoDireto[5];
    int enderecoSimplesIndireto;
    int enderecoDuploIndireto;
    int enderecoTriploIndireto;
};

//utilizado como um intermediario para apontar para mais outros enderecos
struct INodeIndireto
{
    int endereco[MAX_INODEINDIRETO];
    int tamanhoLista;
};

struct Arquivo
{
    int localizacaoINode;
    char nomenclatura[MAX_NOME_ARQUIVO];
};

struct Diretorio
{
    Arquivo arquivo[DIRETORIO_LIMITE_ARQUIVOS];
    int tamanhoLista;
};

struct LinkSimbolico
{
    string rota;
};

//pilha de discos
struct ListaBlocoLivre
{
    int inicio;
    int endereco[QUANTIDADE_LIMITE_ENDERECO_LISTA_BLOCO_LIVRE];
    int enderecoBlocoProx;//endreco do proximo bloco de pilha no disco
};

struct Disco
{
    int bad; //identificador para saber se o bloco esta em bad ou nao

    INode inode;
    INodeIndireto inodeIndireto;
    Diretorio diretorio;
    ListaBlocoLivre lbl;
    LinkSimbolico ls;
};

struct exibicaoEndereco
{
    int endereco;
    char info;
};

//declaracao dos principais metodos
int construirINode(Disco disco[], char tipoArquivo, char privilegio[10], int dimensaoArquivo, int enderecoInodePai, string caminhoLink);
int inserirDiretorioEArquivo(Disco disco[], char tipoArquivo, int enderecoInodeDiretorioAtual, char nomeDiretorioArquivoNovo[MAX_NOME_ARQUIVO], int dimensaoArquivo, string caminhoLink, int enderecoInodeCriado);
void adicionarArquivoNoDiretorio(Disco disco[], int enderecoDiretorio, int enderecoInodeCriado, char nomeDiretorioArquivo[MAX_NOME_ARQUIVO]);
int obterQuantidadeBlocosUtilizar(Disco disco[], int quantidadeBlocosNecessarios);
void obterQuantidadeBlocosPeloMaiorArquivo(Disco disco[], int quantidadeBlocosDisponivel, int &quantidadeUsou, int &quantidadeRealUtilizada);
void abrirArquivoParaEdicao(Disco disco[], int localizacaoINodeAtual, string nomeArquivo, string &enderecosUtilizados);
bool deletarArquivo(Disco disco[], int localizacaoINodeAtual, string nomeArquivo, int primeiraVez);
bool deletarDiretorio(Disco disco[], int localizacaoINodeAtual, string nomeDiretorio, int &contadorDiretorio, int primeiraVez);
int alterarDiretorio(Disco disco[], int localizacaoINodeAtual, string nomeDiretorio, int localizacaoINodeRaiz, string &rotaAbsoluta);
void procurarBlocosDoArquivo(Disco disco[], int enderecoInodeArquivo, string &enderecosUtilizados);
void listaDiretorioAtualIgualExplorer(Disco disco[], int localizacaoINodeAtual, bool primeiraVez);
void mostrarLinksNoDiretorio(Disco disco[], int localizacaoINodeAtual, bool primeiraVez);

//pegando o nomenclatura do dono (usuario)
string obterNomeProprietario(int codigoProprietario)
{
    string nomenclatura;

    switch(codigoProprietario)
    {
	    case 1000:
	        nomenclatura.assign("root");
	        break;
	
	    default:
	        nomenclatura.assign("nao definido");
    }

    return nomenclatura;
}

//pegando nomenclatura do categoria desse dono
string obterNomeGrupo(int codigoProprietario)
{
    string nomenclatura;

    switch (codigoProprietario)
    {
	    case 1000:
	        nomenclatura.assign("root");
	        break;
	    default:
	        nomenclatura.assign("nao definido");
    }

    return nomenclatura;
}

//inicializando disco
void inicializarDisco(disco &disco)
{
    disco.bad = 0; //bad = 0 -> indica que nao tem blocos bads
    disco.diretorio.tamanhoLista = 0; //diretorio vazio
    disco.inode.seguranca[0] = '\0'; //inode sem tipo definido
    disco.inodeIndireto.tamanhoLista = 0;
    disco.lbl.inicio = -1;
    disco.ls.rota[0] = '\0';
}

//inicializacao parcial do disco (sem mexer nos bloco bads)
void inicializarDiscoRestrito(disco &disco)
{
    disco.diretorio.tamanhoLista = 0;
    disco.inode.seguranca[0] = '\0';
    disco.inodeIndireto.tamanhoLista = 0;
    disco.lbl.inicio = -1;
    disco.ls.rota[0] = '\0';
}

//pegando a data e hora atual do sistema (retorna ja formatada: dd/mm/aaaa hh:mm:ss)
void definirDataHoraAtualSistema(char dataAtual[])
{
    time_t t;
    t = time(NULL);
    char data[30];
    strftime(data, sizeof(data), "%d/%m/%Y %H:%M:%S", localtime(&t));
    strcpy(dataAtual, data);
}

//define valor invalido como -1
int obterEnderecoInvalido()
{
    return -1;
}

//atribui o valor invalido a uma variavel
void definirEnderecoInvalido(int &endereco)
{
    endereco = obterEnderecoInvalido();
}

//verifica se o endereco passado e invalido
char verificarSeEnderecoEhInvalido(int endereco)
{
    return endereco == obterEnderecoInvalido();
}

//verifica se o endereco ï¿½ valido
char verificarSeEnderecoEhValido(int endereco)
{
    return !verificarSeEnderecoEhInvalido(endereco);
}

//inicializando pilha de blocos livres
void inicializarListaBlocosLivres(ListaBlocoLivre &lbl)
{
    definirEnderecoInvalido(lbl.inicio);
    definirEnderecoInvalido(lbl.enderecoBlocoProx);
}

//verifica se a pilha de blocos livre esta cheia
char verificarSeListaBlocosLivresEstaCheia(ListaBlocoLivre lbl)
{
    return lbl.inicio == QUANTIDADE_LIMITE_ENDERECO_LISTA_BLOCO_LIVRE - 1;
}

//verifica se a lista de blocos
char verificarSeListaBlocosLivresEstaVazia(ListaBlocoLivre lbl)
{
    return lbl.inicio == obterEnderecoInvalido();
}

//verifica se o endereco esta marcado como um bloco bad
char verificarSeEnderecoEstaDescalibrado(Disco disco[], int endereco)
{
    return disco[endereco].bad;
}

//contador da quantidade de blocos livres existente na pilha
int obterQuantidadeBlocosDisponveis(Disco disco[])
{
    int enderecoProxBloco = ENDERECO_CABECA_LISTA;
    int quantidadeBlocosLivres = 0;

    while(enderecoProxBloco != -1 && disco[enderecoProxBloco].lbl.inicio > -1)
    {
        for(int j = 0; j <= disco[enderecoProxBloco].lbl.inicio; j++)
        {
            if(!disco[disco[enderecoProxBloco].lbl.endereco[j]].bad)
                quantidadeBlocosLivres++;
        }

        enderecoProxBloco = disco[enderecoProxBloco].lbl.enderecoBlocoProx;
    }

    return quantidadeBlocosLivres;
}

//adiciona novo bloco livre na lista
void adicionarBlocoLivreNaLista(Disco disco[], int endereco)
{
    int enderecoBlocoAtual = ENDERECO_CABECA_LISTA;
    int enderecoProxBloco = disco[ENDERECO_CABECA_LISTA].lbl.enderecoBlocoProx;

	//garante que a lista contem mais de um bloco, entao anda ate o final da lista
    while(verificarSeEnderecoEhValido(enderecoProxBloco)) 
    {
        enderecoBlocoAtual = enderecoProxBloco;
        enderecoProxBloco = disco[enderecoProxBloco].lbl.enderecoBlocoProx;
    }

    //se o ultimo bloco encontrado da lista nao esta cheio, adiciona o endereco no final
    if(!verificarSeListaBlocosLivresEstaCheia(disco[enderecoBlocoAtual].lbl))
        disco[enderecoBlocoAtual].lbl.endereco[++disco[enderecoBlocoAtual].lbl.inicio] = endereco;
    else
    {
        //caso estiver cheio, adiciona mais um bloco na lista
        disco[enderecoBlocoAtual].lbl.enderecoBlocoProx = enderecoBlocoAtual + 1;

        //adiciona o endereco
        disco[enderecoBlocoAtual + 1].lbl.endereco[++disco[enderecoBlocoAtual + 1].lbl.inicio] = endereco;
    }
}

//retira um bloco livre da lista
int removerBlocoLivreDaLista(Disco disco[])
{
    int enderecoLivre;
    int enderecoBlocoAtual = ENDERECO_CABECA_LISTA;
    int enderecoProxBloco = disco[enderecoBlocoAtual].lbl.enderecoBlocoProx;

    if (!verificarSeListaBlocosLivresEstaVazia(disco[enderecoBlocoAtual].lbl)) //verifica se tem elementos na lista
    {
        if (verificarSeEnderecoEhInvalido(enderecoProxBloco)) //se for igual a -1, quer dizer que nao ha proximo bloco da lista
        {
            enderecoLivre = disco[ENDERECO_CABECA_LISTA].lbl.endereco[disco[ENDERECO_CABECA_LISTA].lbl.inicio--];

            if (verificarSeEnderecoEstaDescalibrado(disco, enderecoLivre))
                return removerBlocoLivreDaLista(disco);

            return enderecoLivre;
        }
        else
        {
            int enderecoBlocoAnterior = enderecoBlocoAtual;

            while (verificarSeEnderecoEhValido(enderecoProxBloco)) // significa que a lista tem mais de um bloco, entao percorre ate o fim da lista
            {
                enderecoBlocoAnterior = enderecoBlocoAtual;
                enderecoBlocoAtual = enderecoProxBloco;
                enderecoProxBloco = disco[enderecoBlocoAtual].lbl.enderecoBlocoProx;
            }

            enderecoLivre = disco[enderecoBlocoAtual].lbl.endereco[disco[enderecoBlocoAtual].lbl.inicio--];

            // caso o bloco atual nao tenha mais nenhum item apos a remocao, altera o apontamento do anterior para -1
            if(verificarSeListaBlocosLivresEstaVazia(disco[enderecoBlocoAtual].lbl))
            {
                // bloco anterior para de apontar para o bloco atual, quebrando a lista
                definirEnderecoInvalido(disco[enderecoBlocoAnterior].lbl.enderecoBlocoProx);
            }

            if (verificarSeEnderecoEstaDescalibrado(disco, enderecoLivre))
                return removerBlocoLivreDaLista(disco);

            return enderecoLivre;
        }
    }

    return obterEnderecoInvalido();
}

//exibe todos os blocos livres da lista
void mostrarListaBlocosLivres(Disco disco[])
{
    int i = 0, inicio;
    printf("\n");
    int enderecoProxBloco = ENDERECO_CABECA_LISTA;

    while (enderecoProxBloco != -1)
    {
        inicio = disco[enderecoProxBloco].lbl.inicio;
        printf("\n -- Lista de Blocos livres: %d --\n", enderecoProxBloco);

        while (!verificarSeEnderecoEhInvalido(inicio))
        {
            printf(" [ %d ] ", disco[enderecoProxBloco].lbl.endereco[inicio--]);
        }

        printf("\n");
        enderecoProxBloco = disco[enderecoProxBloco].lbl.enderecoBlocoProx;
    }
}

void adicionarEnderecoNaExibicao(exibicaoEndereco enderecos[], int &tamanhoLista, int endereco, char info)
{
    enderecos[tamanhoLista].endereco = endereco;
    enderecos[tamanhoLista++].info = info;
}

int construirINodeIndireto(Disco disco[])
{
    int blocoLivreInodeIndireto = removerBlocoLivreDaLista(disco);
    for (int i = 0; i < MAX_INODEINDIRETO; i++)
    {
        definirEnderecoInvalido(disco[blocoLivreInodeIndireto].inodeIndireto.endereco[i]);
    }

    return blocoLivreInodeIndireto;
}

//utilizado para inserir ao criar o inode
void inserirINodeIndiretoSimples(Disco disco[], int enderecoInodeIndireto, int enderecoInodePrincipal, int &quantidadeBlocosNecessarios, int InseridoPeloTriplo = 0)
{
    int quantidadeBlocosUtilizados = 0;
    //verifica se a quantidade de enderecos no inode indireto nao esta cheio
    if(disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista < MAX_INODEINDIRETO - InseridoPeloTriplo)
    {
        int inicio = disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista;
        // adiciona novos blocos enquanto a quantidade for menor que o necessï¿½rio e que nï¿½o esteja cheio
        while (inicio < quantidadeBlocosNecessarios && inicio < MAX_INODEINDIRETO - InseridoPeloTriplo)
        {
            disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio] = removerBlocoLivreDaLista(disco);
            disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista++;
            quantidadeBlocosUtilizados++;
            inicio++;
        }
    }

    // gera a quantidade ainda faltante.
    quantidadeBlocosNecessarios = quantidadeBlocosNecessarios - quantidadeBlocosUtilizados;

    if (InseridoPeloTriplo && quantidadeBlocosNecessarios > 0)
    {
        char privilegio[10];
        for (int i = 0; i < 10; i++)
        {
            disco[enderecoInodePrincipal].inode.seguranca[i];
        }

        strncpy(privilegio, disco[enderecoInodePrincipal].inode.seguranca + 1, 10);

        disco[enderecoInodeIndireto].inodeIndireto.endereco[disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista] = construirINode(disco,
                                                                                                                        disco[enderecoInodePrincipal].inode.seguranca[0],
                                                                                                                        privilegio,
                                                                                                                        quantidadeBlocosNecessarios * 10,
                                                                                                                        enderecoInodePrincipal, "");

        // conseguiu criar o inode
        if (verificarSeEnderecoEhValido(disco[enderecoInodeIndireto].inodeIndireto.endereco[disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista]))
        {
            disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista++;
        }
    }
}

// utilizado para inserir ao criar o inode
void inserirINodeIndiretoDuplo(Disco disco[], int enderecoInodeIndireto, int enderecoInodePrincipal, int &quantidadeBlocosNecessarios, int InseridoPeloTriplo = 0)
{
    // verifica se a quantidade de Endereï¿½os no inode indireto nï¿½o estï¿½ cheio
    if (disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista < MAX_INODEINDIRETO)
    {
        int inicio = disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista;
        // adiciona novos blocos enquanto a quantidade for menor que o necessï¿½rio e que nï¿½o esteja cheio
        while (inicio < quantidadeBlocosNecessarios && inicio < MAX_INODEINDIRETO)
        {
            if (verificarSeEnderecoEhInvalido(disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio]))
            {
                disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio] = construirINodeIndireto(disco);
            }

            inserirINodeIndiretoSimples(disco,
                                        disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio],
                                        enderecoInodePrincipal,
                                        quantidadeBlocosNecessarios,
                                        InseridoPeloTriplo && inicio + 1 == MAX_INODEINDIRETO);

            disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista++;
            inicio++;
        }
    }
}

// utilizado para inserir ao criar o inode
void inserirINodeIndiretoTriplo(Disco disco[], int enderecoInodeIndireto, int enderecoInodePrincipal, int &quantidadeBlocosNecessarios)
{
    // verifica se a quantidade de Endereï¿½os no inode indireto nï¿½o estï¿½ cheio
    if (disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista < MAX_INODEINDIRETO)
    {
        int inicio = disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista;
        // adiciona novos blocos enquanto a quantidade for menor que o necessï¿½rio e que nï¿½o esteja cheio
        while (inicio < quantidadeBlocosNecessarios && inicio < MAX_INODEINDIRETO)
        {
            if (verificarSeEnderecoEhInvalido(disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio]))
            {
                disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio] = construirINodeIndireto(disco);
            }

            inserirINodeIndiretoDuplo(disco,
                                      disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio],
                                      enderecoInodePrincipal,
                                      quantidadeBlocosNecessarios,
                                      1);

            disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista++;
            inicio++;
        }
    }
}

void preInserirINodeIndiretoSimples(Disco disco[], int &quantidadeBlocosNecessarios, int &quantidadeBlocosUsou, int InseridoPeloTriplo = 0)
{
    int quantidadeBlocosUtilizados = 0;
    // verifica se a quantidade de Endereï¿½os no inode indireto nï¿½o estï¿½ cheio

    int inicio = 0;
    // adiciona novos blocos enquanto a quantidade for menor que o necessï¿½rio e que nï¿½o esteja cheio
    while (inicio < quantidadeBlocosNecessarios && inicio < MAX_INODEINDIRETO - InseridoPeloTriplo)
    {
        quantidadeBlocosUtilizados++;
        inicio++;
    }

    // gera a quantidade ainda faltante.
    quantidadeBlocosNecessarios = quantidadeBlocosNecessarios - quantidadeBlocosUtilizados;
    quantidadeBlocosUsou += quantidadeBlocosUtilizados;
    if (InseridoPeloTriplo && quantidadeBlocosNecessarios > 0)
    {
        quantidadeBlocosUsou++;
        quantidadeBlocosUsou += obterQuantidadeBlocosUtilizar(disco, quantidadeBlocosNecessarios) - 1;
    }
}

// utilizado para inserir ao criar o inode
void preInserirINodeIndiretoDuplo(Disco disco[], int &quantidadeBlocosNecessarios, int &quantidadeBlocosUsou, int InseridoPeloTriplo = 0)
{
    // verifica se a quantidade de Endereï¿½os no inode indireto nï¿½o estï¿½ cheio

    int inicio = 0;
    // adiciona novos blocos enquanto a quantidade for menor que o necessï¿½rio e que nï¿½o esteja cheio
    while (inicio < quantidadeBlocosNecessarios && inicio < MAX_INODEINDIRETO)
    {
        quantidadeBlocosUsou++;
        preInserirINodeIndiretoSimples(disco, quantidadeBlocosNecessarios, quantidadeBlocosUsou,
                                      InseridoPeloTriplo && inicio + 1 == MAX_INODEINDIRETO);

        inicio++;
    }
}

// utilizado para inserir ao criar o inode
void preInserirINodeIndiretoTriplo(Disco disco[], int &quantidadeBlocosNecessarios, int &quantidadeBlocosUsou)
{

    int inicio = 0;
    // adiciona novos blocos enquanto a quantidade for menor que o necessï¿½rio e que nï¿½o esteja cheio
    while (inicio < quantidadeBlocosNecessarios && inicio < MAX_INODEINDIRETO)
    {
        quantidadeBlocosUsou++;
        preInserirINodeIndiretoDuplo(disco, quantidadeBlocosNecessarios, quantidadeBlocosUsou, 1);

        inicio++;
    }
}



void procurarMaiorArqInodeIndiretoSimples(Disco disco[], int quantidadeBlocosDisponivel, int &quantidadeBlocosUsou, int &quantidadeRealUtilizada, int InseridoPeloTriplo = 0)
{
    int quantidadeBlocosUtilizados = 0;
    // verifica se a quantidade de Endereï¿½os no inode indireto nï¿½o estï¿½ cheio

    int inicio = 0;
    // adiciona novos blocos enquanto a quantidade for menor que o necessï¿½rio e que nï¿½o esteja cheio
    while (inicio < quantidadeBlocosDisponivel && quantidadeBlocosUsou <  quantidadeBlocosDisponivel && inicio < MAX_INODEINDIRETO - InseridoPeloTriplo)
    {
        quantidadeBlocosUtilizados++;
        quantidadeRealUtilizada++;
        inicio++;
    }

    // gera a quantidade ainda faltante.    
    quantidadeBlocosUsou += quantidadeBlocosUtilizados;
    if (InseridoPeloTriplo && quantidadeBlocosUsou < quantidadeBlocosDisponivel)
    {
        quantidadeBlocosUsou++;
        obterQuantidadeBlocosPeloMaiorArquivo(disco, quantidadeBlocosDisponivel, quantidadeBlocosUsou, quantidadeRealUtilizada);
        quantidadeBlocosUsou--;
    }
}

// utilizado para inserir ao criar o inode
void procurarMaiorArqInodeIndiretoDuplo(Disco disco[], int quantidadeBlocosDisponivel, int &quantidadeBlocosUsou, int &quantidadeRealUtilizada, int InseridoPeloTriplo = 0)
{
    // verifica se a quantidade de Endereï¿½os no inode indireto nï¿½o estï¿½ cheio

    int inicio = 0;
    // adiciona novos blocos enquanto a quantidade for menor que o necessï¿½rio e que nï¿½o esteja cheio
    while (inicio < quantidadeBlocosDisponivel && quantidadeBlocosUsou <  quantidadeBlocosDisponivel &&inicio < MAX_INODEINDIRETO)
    {
        quantidadeBlocosUsou++;
        procurarMaiorArqInodeIndiretoSimples(disco, quantidadeBlocosDisponivel, quantidadeBlocosUsou, quantidadeRealUtilizada,
                                      InseridoPeloTriplo && inicio + 1 == MAX_INODEINDIRETO);

        inicio++;
    }
}

// utilizado para inserir ao criar o inode
void procurarMaiorArqInodeIndiretoTriplo(Disco disco[], int quantidadeBlocosDisponivel, int &quantidadeBlocosUsou, int &quantidadeRealUtilizada)
{

    int inicio = 0;
    // adiciona novos blocos enquanto a quantidade for menor que o necessï¿½rio e que nï¿½o esteja cheio
    while (inicio < quantidadeBlocosDisponivel &&quantidadeBlocosUsou <  quantidadeBlocosDisponivel && inicio < MAX_INODEINDIRETO)
    {
        quantidadeBlocosUsou++;
        procurarMaiorArqInodeIndiretoDuplo(disco, quantidadeBlocosDisponivel, quantidadeBlocosUsou, quantidadeRealUtilizada, 1);

        inicio++;
    }
}

int obterQuantidadeBlocosUtilizar(Disco disco[], int quantidadeBlocosNecessarios)
{    
    int quantidadeUsou = 0;
    int quantidadeBlocosUtilizados = 0;
    int i = 0;
    while (i < quantidadeBlocosNecessarios && i < 5)
    {
        quantidadeBlocosUtilizados++;
        i++;
    }

    quantidadeUsou += quantidadeBlocosUtilizados;
    int quantidadeBlocosFaltantes = quantidadeBlocosNecessarios - quantidadeBlocosUtilizados;
    if (quantidadeBlocosFaltantes > 0)
    {
        quantidadeUsou++;
        preInserirINodeIndiretoSimples(disco, quantidadeBlocosFaltantes, quantidadeUsou);
    }
    if (quantidadeBlocosFaltantes > 0)
    {
        quantidadeUsou++;
        preInserirINodeIndiretoDuplo(disco, quantidadeBlocosFaltantes, quantidadeUsou);
    }

    if (quantidadeBlocosFaltantes > 0)
    {
        quantidadeUsou++;
        preInserirINodeIndiretoTriplo(disco, quantidadeBlocosFaltantes, quantidadeUsou);
    }

    return quantidadeUsou + 1;
}


void obterQuantidadeBlocosPeloMaiorArquivo(Disco disco[], int quantidadeBlocosDisponivel, int &quantidadeUsou, int &quantidadeRealUtilizada)
{
    int quantidadeBlocosUtilizados = 0;
    int i = 0;
    while (i < quantidadeBlocosDisponivel && quantidadeUsou < quantidadeBlocosDisponivel && i < 5)
    {
        quantidadeBlocosUtilizados++;
        quantidadeRealUtilizada++;
        i++;
    }

    quantidadeUsou += quantidadeBlocosUtilizados;    
   
    if(quantidadeUsou < quantidadeBlocosDisponivel)
    {
        quantidadeUsou++;
        procurarMaiorArqInodeIndiretoSimples(disco, quantidadeBlocosDisponivel, quantidadeUsou, quantidadeRealUtilizada);
    }
  
   

   if(quantidadeUsou <quantidadeBlocosDisponivel)
    {
        quantidadeUsou++;
        procurarMaiorArqInodeIndiretoDuplo(disco, quantidadeBlocosDisponivel, quantidadeUsou, quantidadeRealUtilizada);
    }
    
    
    
    if(quantidadeUsou <quantidadeBlocosDisponivel)
    {
        quantidadeUsou++;
        procurarMaiorArqInodeIndiretoTriplo(disco, quantidadeBlocosDisponivel, quantidadeUsou, quantidadeRealUtilizada);
    }
    
    // quantidadeUsou++;
}

int obterQuantidadeBlocosPerdidos(int quantidadeBlocosTotal){

    int numeroBlocosPerdidos =  quantidadeBlocosTotal/QUANTIDADE_LIMITE_ENDERECO_LISTA_BLOCO_LIVRE;;
    return numeroBlocosPerdidos * 10;
}




// utilizado para exibir o disco
void percorrerINodeIndiretoSimples(Disco disco[], int enderecoInodeIndireto, exibicaoEndereco enderecos[], int &tamanhoLista, int ChamadoPeloTriplo = 0)
{
    int inicio = 0;
    // adiciona novos blocos enquanto a quantidade for menor que o necessï¿½rio e que nï¿½o esteja cheio
    while (inicio < disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista && inicio < MAX_INODEINDIRETO - ChamadoPeloTriplo)
    {
        if (!verificarSeEnderecoEstaDescalibrado(disco, disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio]))
        {
            if (disco[disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio]].ls.rota.length() > 0)
                adicionarEnderecoNaExibicao(enderecos, tamanhoLista, disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio], 'detalhado');
            else
                adicionarEnderecoNaExibicao(enderecos, tamanhoLista, disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio], 'A');
        }

        inicio++;
    }
}

// utilizado para exibir o disco
void percorrerINodeIndiretoDuplo(Disco disco[], int enderecoInodeIndireto, exibicaoEndereco enderecos[], int &tamanhoLista, int ChamadoPeloTriplo = 0)
{
    int inicio = 0;
    // adiciona novos blocos enquanto a quantidade for menor que o necessï¿½rio e que nï¿½o esteja cheio
    while (inicio < disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista && inicio < MAX_INODEINDIRETO)
    {
        if (!verificarSeEnderecoEstaDescalibrado(disco, disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio]))
        {
            adicionarEnderecoNaExibicao(enderecos, tamanhoLista, disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio], 'I');
            percorrerINodeIndiretoSimples(disco, disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio], enderecos, tamanhoLista, ChamadoPeloTriplo && inicio + 1 == MAX_INODEINDIRETO);
        }

        inicio++;
    }
}

// utilizado para exibir o disco
void percorrerINodeIndiretoTriplo(Disco disco[], int enderecoInodeIndireto, exibicaoEndereco enderecos[], int &tamanhoLista)
{
    int inicio = 0;
    // adiciona novos blocos enquanto a quantidade for menor que o necessï¿½rio e que nï¿½o esteja cheio
    while (inicio < disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista && inicio < MAX_INODEINDIRETO)
    {
        if (!verificarSeEnderecoEstaDescalibrado(disco, disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio])){
            adicionarEnderecoNaExibicao(enderecos, tamanhoLista, disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio], 'I');
            percorrerINodeIndiretoDuplo(disco, disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio], enderecos, tamanhoLista, 1);
        }

        inicio++;
    }
}

void procurarOcupacaoInodeIndiretoSimples(Disco disco[], int enderecoInodeIndireto, int &qtdBlocosOcupados, int ChamadoPeloTriplo = 0)
{

    // verifica se a quantidade de Endereï¿½os no inode indireto nï¿½o estï¿½ cheio
    if (disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista > 0)
    {
        int inicio = 0;
        // adiciona novos blocos enquanto a quantidade for menor que o necessï¿½rio e que nï¿½o esteja cheio
        while (inicio < disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista && inicio < MAX_INODEINDIRETO - ChamadoPeloTriplo)
        {
            qtdBlocosOcupados++;
            inicio++;
        }
    }
}

// utilizado para exibir o disco
void procurarOcupacaoInodeIndiretoDuplo(Disco disco[], int enderecoInodeIndireto, int &qtdBlocosOcupados, int ChamadoPeloTriplo = 0)
{

    // verifica se a quantidade de Endereï¿½os no inode indireto nï¿½o estï¿½ cheio
    if (disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista > 0)
    {
        int inicio = 0;
        // adiciona novos blocos enquanto a quantidade for menor que o necessï¿½rio e que nï¿½o esteja cheio
        while (inicio < disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista && inicio < MAX_INODEINDIRETO)
        {
            qtdBlocosOcupados++;
            procurarOcupacaoInodeIndiretoSimples(disco, disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio], qtdBlocosOcupados, ChamadoPeloTriplo && inicio + 1 == MAX_INODEINDIRETO);
            inicio++;
        }
    }
}

// utilizado para exibir o disco
void procurarOcupacaoInodeIndiretoTriplo(Disco disco[], int enderecoInodeIndireto, int &qtdBlocosOcupados)
{

    // verifica se a quantidade de Endereï¿½os no inode indireto nï¿½o estï¿½ cheio
    if (disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista > 0)
    {
        int inicio = 0;
        // adiciona novos blocos enquanto a quantidade for menor que o necessï¿½rio e que nï¿½oo esteja cheio
        while (inicio < disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista && inicio < MAX_INODEINDIRETO)
        {
            qtdBlocosOcupados++;
            procurarOcupacaoInodeIndiretoDuplo(disco, disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio], qtdBlocosOcupados, 1);
            inicio++;
        }
    }
}

// utilizado para exibir os diretï¿½rios do disco
void listarDiretorioINodeIndiretoSimples(Disco disco[], int enderecoInodeIndireto, int &linha, char tipoListagem, bool exibeDiretorioOculto, int ChamadoPeloTriplo = 0)
{
    int enderecoInodeArquivo;
    int i;
    // verifica se a quantidade de Endereï¿½os no inode indireto nï¿½o estï¿½ cheio
    if (disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista > 0)
    {
        int inicio = 0;
        // adiciona novos blocos enquanto a quantidade for menor que o necessï¿½rio e que nï¿½o esteja cheio
        while (inicio < disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista && inicio < MAX_INODEINDIRETO - ChamadoPeloTriplo)
        {

            if (!tipoListagem)
            {
                for (i = 0; i < disco[disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio]].diretorio.tamanhoLista; i++)
                {
                    printf("%s\t", disco[disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio]].diretorio.arquivo[i].nomenclatura);
                }

                if (linha == 10)
                {
                    printf("\n");
                    linha = 0;
                }
            }
            else
            {

                for (i = 0; i < disco[disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio]].diretorio.tamanhoLista; i++)
                {
                    enderecoInodeArquivo = disco[disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio]].diretorio.arquivo[i].localizacaoINode;
                    printf("%s ", disco[enderecoInodeArquivo].inode.seguranca);
                    printf("%d ", disco[enderecoInodeArquivo].inode.quantidadeLinkFisico);
                    printf("%s ", obterNomeProprietario(disco[enderecoInodeArquivo].inode.dono).c_str());
                    printf("%s ", obterNomeGrupo(disco[enderecoInodeArquivo].inode.categoria).c_str());
                    printf("%d ", disco[enderecoInodeArquivo].inode.dimensaoArquivo);
                    printf("%s ", disco[enderecoInodeArquivo].inode.momentoUltimaAlteracao);
                    printf("%s\n", disco[disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio]].diretorio.arquivo[i].nomenclatura);
                }
            }

            inicio++;
        }
    }
}

// utilizado para exibir os diretï¿½rios do disco
void listarDiretorioINodeIndiretoDuplo(Disco disco[], int enderecoInodeIndireto, int &linha, char tipoListagem, bool exibeDiretorioOculto, int ChamadoPeloTriplo = 0)
{

    // verifica se a quantidade de Endereï¿½os no inode indireto nï¿½o estï¿½ cheio
    if (disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista > 0)
    {
        int inicio = 0;
        // adiciona novos blocos enquanto a quantidade for menor que o necessï¿½rio e que nï¿½o esteja cheio
        while (inicio < disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista && inicio < MAX_INODEINDIRETO)
        {
            listarDiretorioINodeIndiretoSimples(disco, disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio], linha, tipoListagem, exibeDiretorioOculto, ChamadoPeloTriplo && inicio + 1 == MAX_INODEINDIRETO);
            inicio++;
        }
    }
}

// utilizado para exibir os diretï¿½rios do disco
void listarDiretorioINodeIndiretoTriplo(Disco disco[], int enderecoInodeIndireto, int &linha, char tipoListagem, bool exibeDiretorioOculto, int ChamadoPeloTriplo = 0)
{

    // verifica se a quantidade de Endereï¿½os no inode indireto nï¿½o estï¿½ cheio
    if (disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista > 0)
    {
        int inicio = 0;
        // adiciona novos blocos enquanto a quantidade for menor que o necessï¿½rio e que nï¿½o esteja cheio
        while (inicio < disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista && inicio < MAX_INODEINDIRETO)
        {
            listarDiretorioINodeIndiretoDuplo(disco, disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio], linha, tipoListagem, exibeDiretorioOculto, 1);
            inicio++;
        }
    }
}

// utilizado para exibir os diretï¿½rios do disco
int verificarExistenciaArquivoOuDiretorioInodeIndiretoSimples(Disco disco[], int enderecoInodeIndireto, char tipoArquivo, string nomeArquivo, int ChamadoPeloTriplo = 0)
{
    int enderecoInodeArquivo;
    int i;
    int inicio = 0;

    if (tipoArquivo == ' ')
    {
        // adiciona novos blocos enquanto a quantidade for menor que o necessï¿½rio e que nï¿½o esteja cheio
        while (inicio < disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista && inicio < MAX_INODEINDIRETO - ChamadoPeloTriplo)
        {
            for (i = 0; i < disco[disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio]].diretorio.tamanhoLista; i++)
            {
                if (strcmp(disco[disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio]].diretorio.arquivo[i].nomenclatura, nomeArquivo.c_str()) == 0)
                {
                    return disco[disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio]].diretorio.arquivo[i].localizacaoINode;
                }
            }

            inicio++;
        }
    }
    else
    {
        // adiciona novos blocos enquanto a quantidade for menor que o necessï¿½rio e que nï¿½o esteja cheio
        while (inicio < disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista && inicio < MAX_INODEINDIRETO - ChamadoPeloTriplo)
        {
            for (i = 0; i < disco[disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio]].diretorio.tamanhoLista; i++)
            {
                if (disco[disco[disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio]].diretorio.arquivo[i].localizacaoINode].inode.seguranca[0] == tipoArquivo)
                {
                    if (strcmp(disco[disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio]].diretorio.arquivo[i].nomenclatura, nomeArquivo.c_str()) == 0)
                    {
                        return disco[disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio]].diretorio.arquivo[i].localizacaoINode;
                    }
                }
            }

            inicio++;
        }
    }

    return obterEnderecoInvalido();
}

// utilizado para exibir os diretï¿½rios do disco
int verificarExistenciaArquivoOuDiretorioInodeIndiretoDuplo(Disco disco[], int enderecoInodeIndireto, char tipoArquivo, string nomeArquivo, int ChamadoPeloTriplo = 0)
{
    int enderecoInodeDir;

    // verifica se a quantidade de Endereï¿½os no inode indireto nï¿½o estï¿½ cheio
    if (disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista > 0)
    {
        int inicio = 0;
        // adiciona novos blocos enquanto a quantidade for menor que o necessï¿½rio e que nï¿½o esteja cheio
        while (inicio < disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista && inicio < MAX_INODEINDIRETO)
        {
            enderecoInodeDir = verificarExistenciaArquivoOuDiretorioInodeIndiretoSimples(disco, disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio], tipoArquivo, nomeArquivo, ChamadoPeloTriplo && inicio + 1 == MAX_INODEINDIRETO);
            if (verificarSeEnderecoEhValido(enderecoInodeDir))
                return enderecoInodeDir;

            inicio++;
        }
    }

    return obterEnderecoInvalido();
}

// utilizado para exibir os diretï¿½rios do disco
int verificarExistenciaArquivoOuDiretorioInodeIndiretoTriplo(Disco disco[], int enderecoInodeIndireto, char tipoArquivo, string nomeArquivo, int ChamadoPeloTriplo = 0)
{
    int enderecoInodeDir;

    // verifica se a quantidade de Endereï¿½os no inode indireto nï¿½o estï¿½ cheio
    if (disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista > 0)
    {
        int inicio = 0;
        // adiciona novos blocos enquanto a quantidade for menor que o necessï¿½rio e que nï¿½o esteja cheio
        while (inicio < disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista && inicio < MAX_INODEINDIRETO)
        {
            enderecoInodeDir = verificarExistenciaArquivoOuDiretorioInodeIndiretoDuplo(disco, disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio], tipoArquivo, nomeArquivo, 1);
            if (verificarSeEnderecoEhValido(enderecoInodeDir))
                return enderecoInodeDir;

            inicio++;
        }
    }

    return obterEnderecoInvalido();
}

int procurarEnderecoEntradaDiretorioArquivoInodeIndiretoSimples(Disco disco[], int enderecoInodeIndireto, char tipoArquivo, string nomeArquivo, int ChamadoPeloTriplo = 0)
{
    int enderecoInodeArquivo;
    int i;
    int inicio = 0;

    if (tipoArquivo == ' ')
    {
        // adiciona novos blocos enquanto a quantidade for menor que o necessï¿½rio e que nï¿½o esteja cheio
        while (inicio < disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista && inicio < MAX_INODEINDIRETO - ChamadoPeloTriplo)
        {
            for (i = 0; i < disco[disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio]].diretorio.tamanhoLista; i++)
            {
                if (strcmp(disco[disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio]].diretorio.arquivo[i].nomenclatura, nomeArquivo.c_str()) == 0)
                {
                    return disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio];
                }
            }
            inicio++;
        }
    }
    else
    {
        // adiciona novos blocos enquanto a quantidade for menor que o necessï¿½rio e que nï¿½o esteja cheio
        while (inicio < disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista && inicio < MAX_INODEINDIRETO - ChamadoPeloTriplo)
        {
            for (i = 0; i < disco[disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio]].diretorio.tamanhoLista; i++)
            {
                if (disco[disco[disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio]].diretorio.arquivo[i].localizacaoINode].inode.seguranca[0] == tipoArquivo)
                {
                    if (strcmp(disco[disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio]].diretorio.arquivo[i].nomenclatura, nomeArquivo.c_str()) == 0)
                    {
                        return disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio];
                    }
                }
            }
            inicio++;
        }
    }

    //TODO - CHAMAR INODE AUXILIAR SE Nï¿½O ENCONTRAR
    return obterEnderecoInvalido();
}

// utilizado para exibir os diretï¿½rios do disco
int procurarEnderecoEntradaDiretorioArquivoInodeIndiretoDuplo(Disco disco[], int enderecoInodeIndireto, char tipoArquivo, string nomeArquivo, int ChamadoPeloTriplo = 0)
{
    int enderecoInodeDir;

    // verifica se a quantidade de Endereï¿½os no inode indireto nï¿½o estï¿½ cheio
    if (disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista > 0)
    {
        int inicio = 0;
        // adiciona novos blocos enquanto a quantidade for menor que o necessï¿½rio e que nï¿½o esteja cheio
        while (inicio < disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista && inicio < MAX_INODEINDIRETO)
        {
            enderecoInodeDir = procurarEnderecoEntradaDiretorioArquivoInodeIndiretoSimples(disco, disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio], tipoArquivo, nomeArquivo, ChamadoPeloTriplo && inicio + 1 == MAX_INODEINDIRETO);
            if (verificarSeEnderecoEhValido(enderecoInodeDir))
                return enderecoInodeDir;

            inicio++;
        }
    }

    return obterEnderecoInvalido();
}
// utilizado para exibir os diretï¿½rios do disco
int procurarEnderecoEntradaDiretorioArquivoInodeIndiretoTriplo(Disco disco[], int enderecoInodeIndireto, char tipoArquivo, string nomeArquivo, int ChamadoPeloTriplo = 0)
{
    int enderecoInodeDir;

    // verifica se a quantidade de Endereï¿½os no inode indireto nï¿½o estï¿½ cheio
    if (disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista > 0)
    {
        int inicio = 0;
        // adiciona novos blocos enquanto a quantidade for menor que o necessï¿½rio e que nï¿½o esteja cheio
        while (inicio < disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista && inicio < MAX_INODEINDIRETO)
        {
            enderecoInodeDir = procurarEnderecoEntradaDiretorioArquivoInodeIndiretoDuplo(disco, disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio], tipoArquivo, nomeArquivo, 1);
            if (verificarSeEnderecoEhValido(enderecoInodeDir))
                return enderecoInodeDir;

            inicio++;
        }
    }

    return obterEnderecoInvalido();
}

// utilizado para reservar novo bloco do diretï¿½rio apï¿½s criaï¿½ï¿½o do inode
int procurarNovoBlocoDiretorioLivreEmIndiretoSimples(Disco disco[], int enderecoInodeIndireto, int enderecoInodeDiretorioAtual, int InseridoPeloTriplo = 0)
{
    bool consegueInserir = false;
    int i;

    int endereco = disco[enderecoInodeIndireto].inodeIndireto.endereco[0];

    for (i = 0; i < MAX_INODEINDIRETO && verificarSeEnderecoEhValido(endereco) && !consegueInserir; i++)
    {
        endereco = disco[enderecoInodeIndireto].inodeIndireto.endereco[i];

        if (!verificarSeEnderecoEhInvalido(endereco) && disco[endereco].diretorio.tamanhoLista < 10)
        {
            consegueInserir = true;
        }
    }

    // signfica que nï¿½o conseguiu inserir e nï¿½o percorreu todas as funï¿½ï¿½es, senso assim, a prï¿½xima posiï¿½ï¿½o ï¿½ -1, entï¿½o deve ser criado um novo diretï¿½rio
    if (!consegueInserir && i <= MAX_INODEINDIRETO && verificarSeEnderecoEhInvalido(endereco))
    {
        endereco = removerBlocoLivreDaLista(disco);
        disco[enderecoInodeIndireto].inodeIndireto.endereco[i - (i == 0 ? 0 : 1)] = endereco;
        disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista++;
        disco[disco[disco[enderecoInodeDiretorioAtual].inode.enderecoDireto[0]].diretorio.arquivo[0].localizacaoINode].inode.dimensaoArquivo++;
    }
    else if (!consegueInserir) // chegou ao ï¿½ltimo endereï¿½o e mesmo assim nï¿½o conseguiu inserir
    {
        return obterEnderecoInvalido();
    }

    // TODO - VERIFICAR SE estï¿½ SENDO PERCORRIDO PELO TRIPLO, E SE O ï¿½ltimo endereï¿½o INDIRETO ï¿½ OQ SOBROU, SENDO ASSIM, DEVE SE CRIAR UM INODE EXTRA
    return endereco;
}

// utilizado para reservar novo bloco do diretï¿½rio apï¿½s criaï¿½ï¿½o do inode
int procurarNovoBlocoDiretorioLivreEmIndiretoDuplo(Disco disco[], int enderecoInodeIndireto, int enderecoInodeDiretorioAtual, int InseridoPeloTriplo = 0)
{
    bool conseguiuInserir = false;
    int endereco = obterEnderecoInvalido();

    // verifica se a quantidade de Endereï¿½os no inode indireto nï¿½o estï¿½ cheio
    int inicio = 0;
    while (inicio < MAX_INODEINDIRETO && !conseguiuInserir)
    {
        if (!verificarSeEnderecoEhInvalido(disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio]))
        {
            endereco = procurarNovoBlocoDiretorioLivreEmIndiretoSimples(disco,
                                                                     disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio],
                                                                     InseridoPeloTriplo && inicio + 1 == MAX_INODEINDIRETO);
            if (verificarSeEnderecoEhValido(endereco))
                conseguiuInserir = true;
        }

        if (verificarSeEnderecoEhInvalido(endereco) && verificarSeEnderecoEhInvalido(disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio]))
        {
            if (disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista < MAX_INODEINDIRETO)
            {
                disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio] = construirINodeIndireto(disco);
                disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista++;
                disco[disco[disco[enderecoInodeDiretorioAtual].inode.enderecoDireto[0]].diretorio.arquivo[0].localizacaoINode].inode.dimensaoArquivo++;
                endereco = procurarNovoBlocoDiretorioLivreEmIndiretoSimples(disco,
                                                                         disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio],
                                                                         InseridoPeloTriplo && inicio + 1 == MAX_INODEINDIRETO);
                conseguiuInserir = true;
            }
            else
                break;
        }

        inicio++;
    }

    if (conseguiuInserir)
        return endereco;

    return obterEnderecoInvalido();
}

// utilizado para reservar novo bloco apï¿½s criaï¿½ï¿½o do inode
int procurarNovoBlocoDiretorioLivreEmIndiretoTriplo(Disco disco[], int enderecoInodeIndireto, int enderecoInodeDiretorioAtual)
{
    bool conseguiuInserir = false;
    int endereco = obterEnderecoInvalido();

    // verifica se a quantidade de Endereï¿½os no inode indireto nï¿½o estï¿½ cheio
    int inicio = 0;
    while (inicio < MAX_INODEINDIRETO && !conseguiuInserir)
    {
        if (!verificarSeEnderecoEhInvalido(disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio]))
        {
            endereco = procurarNovoBlocoDiretorioLivreEmIndiretoDuplo(disco,
                                                                   disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio], 1);
            if (verificarSeEnderecoEhValido(endereco))
                conseguiuInserir = true;
        }

        if (verificarSeEnderecoEhInvalido(endereco) && verificarSeEnderecoEhInvalido(disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio]))
        {
            if (disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista < MAX_INODEINDIRETO)
            {
                disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio] = construirINodeIndireto(disco);
                disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista++;
                disco[disco[disco[enderecoInodeDiretorioAtual].inode.enderecoDireto[0]].diretorio.arquivo[0].localizacaoINode].inode.dimensaoArquivo++;
                endereco = procurarNovoBlocoDiretorioLivreEmIndiretoDuplo(disco,
                                                                       disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio], 1);
                conseguiuInserir = true;
            }
            else
                break;
        }

        inicio++;
    }

    if (conseguiuInserir)
        return endereco;

    return obterEnderecoInvalido();
}

// utilizado para exibir os blocos do arquivo
void retornarEnderecosINodeIndiretoSimples(Disco disco[], int enderecoInodeIndireto, string &enderecosUtilizados, int ChamadoPeloTriplo = 0)
{
    char enderecoToChar[300];
    int inicio = 0;
    // adiciona novos blocos enquanto a quantidade for menor que o necessï¿½rio e que nï¿½o esteja cheio
    while (inicio < disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista && inicio < MAX_INODEINDIRETO - ChamadoPeloTriplo)
    {
        itoa(disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio], enderecoToChar, 10);
        enderecosUtilizados.append(enderecoToChar).append("-");

        inicio++;
    }

    if (ChamadoPeloTriplo)
    {
        if (verificarSeEnderecoEhValido(disco[enderecoInodeIndireto].inodeIndireto.endereco[MAX_INODEINDIRETO - 1]))
        {
            string nomeArq;
            nomeArq.assign("");

            abrirArquivoParaEdicao(disco, disco[enderecoInodeIndireto].inodeIndireto.endereco[MAX_INODEINDIRETO - 1], nomeArq, enderecosUtilizados);
        }
    }
}

// utilizado para exibir os blocos do arquivo
void retornarEnderecosINodeIndiretoDuplo(Disco disco[], int enderecoInodeIndireto, string &enderecosUtilizados, int ChamadoPeloTriplo = 0)
{
    char enderecoToChar[300];
    int inicio = 0;
    // adiciona novos blocos enquanto a quantidade for menor que o necessï¿½rio e que nï¿½o esteja cheio
    while (inicio < disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista && inicio < MAX_INODEINDIRETO)
    {
        itoa(disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio], enderecoToChar, 10);
        enderecosUtilizados.append(enderecoToChar).append("-");

        retornarEnderecosINodeIndiretoSimples(disco, disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio], enderecosUtilizados, ChamadoPeloTriplo && inicio + 1 == MAX_INODEINDIRETO);
        inicio++;
    }
}

// utilizado para exibir os blocos do arquivo
void retornarEnderecosINodeIndiretoTriplo(Disco disco[], int enderecoInodeIndireto, string &enderecosUtilizados)
{
    char enderecoToChar[300];
    int inicio = 0;
    // adiciona novos blocos enquanto a quantidade for menor que o necessï¿½rio e que nï¿½o esteja cheio
    while (inicio < disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista && inicio < MAX_INODEINDIRETO)
    {
        itoa(disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio], enderecoToChar, 10);
        enderecosUtilizados.append(enderecoToChar).append("-");

        retornarEnderecosINodeIndiretoDuplo(disco, disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio], enderecosUtilizados, 1);
        inicio++;
    }
}

// utilizado para remover os blocos do arquivo
void removerEnderecosINodeIndiretoSimples(Disco disco[], int enderecoInodeIndireto, int ChamadoPeloTriplo = 0)
{
    int inicio = 0;
    // adiciona novos blocos enquanto a quantidade for menor que o necessï¿½rio e que nï¿½o esteja cheio
    while (inicio < disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista && inicio < MAX_INODEINDIRETO - ChamadoPeloTriplo)
    {
        inicializarDisco(disco[disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio]]);
        adicionarBlocoLivreNaLista(disco, disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio]);
        disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio] = obterEnderecoInvalido();
        inicio++;
    }

    if (ChamadoPeloTriplo)
    {
        if (verificarSeEnderecoEhValido(disco[enderecoInodeIndireto].inodeIndireto.endereco[MAX_INODEINDIRETO - 1]))
        {
            string nomeArq;
            nomeArq.assign("");

            deletarArquivo(disco, disco[enderecoInodeIndireto].inodeIndireto.endereco[MAX_INODEINDIRETO - 1], nomeArq, 0);

            inicializarDisco(disco[disco[enderecoInodeIndireto].inodeIndireto.endereco[MAX_INODEINDIRETO - 1]]);
            adicionarBlocoLivreNaLista(disco, disco[enderecoInodeIndireto].inodeIndireto.endereco[MAX_INODEINDIRETO - 1]);
            disco[enderecoInodeIndireto].inodeIndireto.endereco[MAX_INODEINDIRETO - 1] = obterEnderecoInvalido();
        }
    }
}

// utilizado para exibir os blocos do arquivo
void removerEnderecosINodeIndiretoDuplo(Disco disco[], int enderecoInodeIndireto, int ChamadoPeloTriplo = 0)
{
    int inicio = 0;
    // adiciona novos blocos enquanto a quantidade for menor que o necessï¿½rio e que nï¿½o esteja cheio
    while (inicio < disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista && inicio < MAX_INODEINDIRETO)
    {
        removerEnderecosINodeIndiretoSimples(disco, disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio], ChamadoPeloTriplo && inicio + 1 == MAX_INODEINDIRETO);

        inicializarDisco(disco[disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio]]);
        adicionarBlocoLivreNaLista(disco, disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio]);
        disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio] = obterEnderecoInvalido();
        inicio++;
    }
}

// utilizado para exibir os blocos do arquivo
void removerEnderecosINodeIndiretoTriplo(Disco disco[], int enderecoInodeIndireto)
{

    int inicio = 0;
    // adiciona novos blocos enquanto a quantidade for menor que o necessï¿½rio e que nï¿½o esteja cheio
    while (inicio < disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista && inicio < MAX_INODEINDIRETO)
    {
        removerEnderecosINodeIndiretoDuplo(disco, disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio], 1);

        inicializarDisco(disco[disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio]]);
        adicionarBlocoLivreNaLista(disco, disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio]);

        disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio] = obterEnderecoInvalido();
        inicio++;
    }
}

// utilizado para exibir os diretï¿½rios do disco
void retornarQuantidadeDiretorioINodeIndiretoSimples(Disco disco[], int enderecoInodeIndireto, int &contadorDiretorio, int ChamadoPeloTriplo = 0)
{
    int enderecoInodeArquivo;
    int i;
    int inicio = 0;

    // adiciona novos blocos enquanto a quantidade for menor que o necessï¿½rio e que nï¿½o esteja cheio
    while (inicio < disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista && inicio < MAX_INODEINDIRETO - ChamadoPeloTriplo)
    {
        contadorDiretorio += disco[disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio]].diretorio.tamanhoLista;
        inicio++;
    }

    if (ChamadoPeloTriplo)
    {
        if (verificarSeEnderecoEhValido(disco[enderecoInodeIndireto].inodeIndireto.endereco[MAX_INODEINDIRETO - 1]))
        {
            string nomeArq;
            nomeArq.assign("");

            deletarDiretorio(disco, disco[enderecoInodeIndireto].inodeIndireto.endereco[MAX_INODEINDIRETO - 1], nomeArq, contadorDiretorio, 0);
        }
    }
}

// utilizado para exibir os diretï¿½rios do disco
void retornarQuantidadeDiretorioINodeIndiretoDuplo(Disco disco[], int enderecoInodeIndireto, int &contadorDiretorio, int ChamadoPeloTriplo = 0)
{
    int enderecoInodeDir;

    // verifica se a quantidade de Endereï¿½os no inode indireto nï¿½o estï¿½ cheio
    if (disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista > 0)
    {
        int inicio = 0;
        // adiciona novos blocos enquanto a quantidade for menor que o necessï¿½rio e que nï¿½o esteja cheio
        while (inicio < disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista && inicio < MAX_INODEINDIRETO)
        {
            retornarQuantidadeDiretorioINodeIndiretoSimples(disco, disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio], contadorDiretorio, ChamadoPeloTriplo && inicio + 1 == MAX_INODEINDIRETO);
            inicio++;
        }
    }
}

// utilizado para exibir os diretï¿½rios do disco
void retornarQuantidadeDiretorioINodeIndiretoTriplo(Disco disco[], int enderecoInodeIndireto, int &contadorDiretorio, int ChamadoPeloTriplo = 0)
{
    int enderecoInodeDir;
    // verifica se a quantidade de Endereï¿½os no inode indireto nï¿½o estï¿½ cheio
    if (disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista > 0)
    {
        int inicio = 0;
        // adiciona novos blocos enquanto a quantidade for menor que o necessï¿½rio e que nï¿½o esteja cheio
        while (inicio < disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista && inicio < MAX_INODEINDIRETO)
        {
            retornarQuantidadeDiretorioINodeIndiretoDuplo(disco, disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio], contadorDiretorio, 1);
            inicio++;
        }
    }
}

int verificarExistenciaArquivoOuDiretorio(Disco disco[], int localizacaoINodeAtual, string nomeArquivo, char tipoArquivo = ' ', bool igualTipoFornecido=true)
{
    int direto, i, endereco = obterEnderecoInvalido();

    if (tipoArquivo == ' ')
    {
        for (direto = 0; direto < 5 && verificarSeEnderecoEhValido(disco[localizacaoINodeAtual].inode.enderecoDireto[direto]); direto++)
        {

            for (i = 0; i < disco[disco[localizacaoINodeAtual].inode.enderecoDireto[direto]].diretorio.tamanhoLista; i++)
            {
                // printf("\n %s -- %s \n", disco[disco[localizacaoINodeAtual].inode.enderecoDireto[direto]].diretorio.arquivo[i].nomenclatura,nomeDiretorio.c_str());
                if (strcmp(disco[disco[localizacaoINodeAtual].inode.enderecoDireto[direto]].diretorio.arquivo[i].nomenclatura, nomeArquivo.c_str()) == 0)
                {
                    return disco[disco[localizacaoINodeAtual].inode.enderecoDireto[direto]].diretorio.arquivo[i].localizacaoINode;
                }
            }
        }
    }
    else
    {
        if (igualTipoFornecido)
        {
            for (direto = 0; direto < 5 && verificarSeEnderecoEhValido(disco[localizacaoINodeAtual].inode.enderecoDireto[direto]); direto++)
            {

                for (i = 0; i < disco[disco[localizacaoINodeAtual].inode.enderecoDireto[direto]].diretorio.tamanhoLista; i++)
                {
                    if (disco[disco[disco[localizacaoINodeAtual].inode.enderecoDireto[direto]].diretorio.arquivo[i].localizacaoINode].inode.seguranca[0] == tipoArquivo)
                    {
                        // printf("\n %s -- %s \n", disco[disco[localizacaoINodeAtual].inode.enderecoDireto[direto]].diretorio.arquivo[i].nomenclatura,nomeDiretorio.c_str());
                        if (strcmp(disco[disco[localizacaoINodeAtual].inode.enderecoDireto[direto]].diretorio.arquivo[i].nomenclatura, nomeArquivo.c_str()) == 0)
                        {
                            return disco[disco[localizacaoINodeAtual].inode.enderecoDireto[direto]].diretorio.arquivo[i].localizacaoINode;
                        }
                    }
                }
            }
        }
        else
        {
            for (direto = 0; direto < 5 && verificarSeEnderecoEhValido(disco[localizacaoINodeAtual].inode.enderecoDireto[direto]); direto++)
            {

                for (i = 0; i < disco[disco[localizacaoINodeAtual].inode.enderecoDireto[direto]].diretorio.tamanhoLista; i++)
                {
                    if (disco[disco[disco[localizacaoINodeAtual].inode.enderecoDireto[direto]].diretorio.arquivo[i].localizacaoINode].inode.seguranca[0] != tipoArquivo)
                    {
                        // printf("\n %s -- %s \n", disco[disco[localizacaoINodeAtual].inode.enderecoDireto[direto]].diretorio.arquivo[i].nomenclatura,nomeDiretorio.c_str());
                        if (strcmp(disco[disco[localizacaoINodeAtual].inode.enderecoDireto[direto]].diretorio.arquivo[i].nomenclatura, nomeArquivo.c_str()) == 0)
                        {
                            return disco[disco[localizacaoINodeAtual].inode.enderecoDireto[direto]].diretorio.arquivo[i].localizacaoINode;
                        }
                    }
                }
            }
        }
    }

    if (!verificarSeEnderecoEhInvalido(disco[localizacaoINodeAtual].inode.enderecoSimplesIndireto))
    {
        endereco = verificarExistenciaArquivoOuDiretorioInodeIndiretoSimples(disco, disco[localizacaoINodeAtual].inode.enderecoSimplesIndireto, tipoArquivo, nomeArquivo);
    }

    if (verificarSeEnderecoEhValido(endereco))
    {
        return endereco;
    }

    if (!verificarSeEnderecoEhInvalido(disco[localizacaoINodeAtual].inode.enderecoDuploIndireto))
    {
        endereco = verificarExistenciaArquivoOuDiretorioInodeIndiretoDuplo(disco, disco[localizacaoINodeAtual].inode.enderecoDuploIndireto, tipoArquivo, nomeArquivo);
    }

    if (verificarSeEnderecoEhValido(endereco))
    {
        return endereco;
    }

    if (!verificarSeEnderecoEhInvalido(disco[localizacaoINodeAtual].inode.enderecoTriploIndireto))
    {
        endereco = verificarExistenciaArquivoOuDiretorioInodeIndiretoTriplo(disco, disco[localizacaoINodeAtual].inode.enderecoTriploIndireto, tipoArquivo, nomeArquivo, 1);
    }

    if (verificarSeEnderecoEhValido(endereco))
    {
        return endereco;
    }

    return endereco;
}

int procurarEnderecoEntradaDiretorioArquivo(Disco disco[], int localizacaoINodeAtual, string nomeArquivo, char tipoArquivo=' ')
{
    int direto, i, endereco = obterEnderecoInvalido();

    if (tipoArquivo == ' ')
    {
        for (direto = 0; direto < 5 && verificarSeEnderecoEhValido(disco[localizacaoINodeAtual].inode.enderecoDireto[direto]); direto++)
        {
            for (i = 0; i < disco[disco[localizacaoINodeAtual].inode.enderecoDireto[direto]].diretorio.tamanhoLista; i++)
            {
                // printf("\n %s -- %s \n", disco[disco[localizacaoINodeAtual].inode.enderecoDireto[direto]].diretorio.arquivo[i].nomenclatura,nomeDiretorio.c_str());
                if (strcmp(disco[disco[localizacaoINodeAtual].inode.enderecoDireto[direto]].diretorio.arquivo[i].nomenclatura, nomeArquivo.c_str()) == 0)
                {
                    return disco[localizacaoINodeAtual].inode.enderecoDireto[direto];
                }
            }
        }
    }
    else
    {
        for (direto = 0; direto < 5 && verificarSeEnderecoEhValido(disco[localizacaoINodeAtual].inode.enderecoDireto[direto]); direto++)
        {
            for (i = 0; i < disco[disco[localizacaoINodeAtual].inode.enderecoDireto[direto]].diretorio.tamanhoLista; i++)
            {
                if (disco[disco[disco[localizacaoINodeAtual].inode.enderecoDireto[direto]].diretorio.arquivo[i].localizacaoINode].inode.seguranca[0] == tipoArquivo)
                {
                    // printf("\n %s -- %s \n", disco[disco[localizacaoINodeAtual].inode.enderecoDireto[direto]].diretorio.arquivo[i].nomenclatura,nomeDiretorio.c_str());
                    if (strcmp(disco[disco[localizacaoINodeAtual].inode.enderecoDireto[direto]].diretorio.arquivo[i].nomenclatura, nomeArquivo.c_str()) == 0)
                    {
                        return disco[localizacaoINodeAtual].inode.enderecoDireto[direto];
                    }
                }
            }
        }
    }
    

    if (!verificarSeEnderecoEhInvalido(disco[localizacaoINodeAtual].inode.enderecoSimplesIndireto))
    {
        endereco = procurarEnderecoEntradaDiretorioArquivoInodeIndiretoSimples(disco, disco[localizacaoINodeAtual].inode.enderecoSimplesIndireto, tipoArquivo, nomeArquivo);
    }

    if (verificarSeEnderecoEhValido(endereco))
    {
        return endereco;
    }

    if (!verificarSeEnderecoEhInvalido(disco[localizacaoINodeAtual].inode.enderecoDuploIndireto))
    {
        endereco = procurarEnderecoEntradaDiretorioArquivoInodeIndiretoDuplo(disco, disco[localizacaoINodeAtual].inode.enderecoDuploIndireto, tipoArquivo, nomeArquivo);
    }

    if (verificarSeEnderecoEhValido(endereco))
    {
        return endereco;
    }

    if (!verificarSeEnderecoEhInvalido(disco[localizacaoINodeAtual].inode.enderecoTriploIndireto))
    {
        endereco = procurarEnderecoEntradaDiretorioArquivoInodeIndiretoTriplo(disco, disco[localizacaoINodeAtual].inode.enderecoTriploIndireto, tipoArquivo, nomeArquivo, 1);
    }

    if (verificarSeEnderecoEhValido(endereco))
    {
        return endereco;
    }

    return endereco;
}

int construirINode(Disco disco[], char tipoArquivo, char privilegio[10], int dimensaoArquivo = 1, int enderecoInodePai = -1, string caminhoLink = "")
{
    int quantidadeBlocosNecessarios = (int)ceil((double)dimensaoArquivo / (double)10);

    // quantidade de blocos necessï¿½rias ï¿½ maior que a quantidade de blocos livres. Se for, nï¿½o posso gerar o arquivo, entï¿½o retorna "-1"
    if (quantidadeBlocosNecessarios > obterQuantidadeBlocosDisponveis(disco))
    {
        return obterEnderecoInvalido();
    }

    int localizacaoINode = removerBlocoLivreDaLista(disco);
    if (verificarSeEnderecoEhValido(localizacaoINode))
    {
        // inicializa bloco
        for (int i = 0; i < 5; i++)
        {
            definirEnderecoInvalido(disco[localizacaoINode].inode.enderecoDireto[i]);
        }
        definirEnderecoInvalido(disco[localizacaoINode].inode.enderecoSimplesIndireto);
        definirEnderecoInvalido(disco[localizacaoINode].inode.enderecoDuploIndireto);
        definirEnderecoInvalido(disco[localizacaoINode].inode.enderecoTriploIndireto);

        disco[localizacaoINode].inode.seguranca[0] = tipoArquivo;
        strncpy(disco[localizacaoINode].inode.seguranca + 1, privilegio, 10);

        disco[localizacaoINode].inode.quantidadeLinkFisico = 1;
        disco[localizacaoINode].inode.dimensaoArquivo = dimensaoArquivo;

        definirDataHoraAtualSistema(disco[localizacaoINode].inode.momentoCriacao);
        definirDataHoraAtualSistema(disco[localizacaoINode].inode.momentoUltimaAlteracao);
        definirDataHoraAtualSistema(disco[localizacaoINode].inode.momentoUltimoAcesso);

        // usuï¿½rio root
        disco[localizacaoINode].inode.dono = 1000;
        // categoria root
        disco[localizacaoINode].inode.categoria = 1000;

        int quantidadeBlocosUtilizados = 0;

        int i = 0;
        while (i < quantidadeBlocosNecessarios && i < 5)
        {
            if (verificarSeEnderecoEhInvalido(disco[localizacaoINode].inode.enderecoDireto[quantidadeBlocosUtilizados]))
            {
                disco[localizacaoINode].inode.enderecoDireto[quantidadeBlocosUtilizados] = removerBlocoLivreDaLista(disco);
                quantidadeBlocosUtilizados++;
            }
            i++;
        }

        int quantidadeBlocosFaltantes = quantidadeBlocosNecessarios - quantidadeBlocosUtilizados;
        if (quantidadeBlocosFaltantes > 0)
        {
            if (verificarSeEnderecoEhInvalido(disco[localizacaoINode].inode.enderecoSimplesIndireto))
            {
                disco[localizacaoINode].inode.enderecoSimplesIndireto = construirINodeIndireto(disco);
            }
            inserirINodeIndiretoSimples(disco, disco[localizacaoINode].inode.enderecoSimplesIndireto, localizacaoINode, quantidadeBlocosFaltantes);
        }

        if (quantidadeBlocosFaltantes > 0)
        {
            if (verificarSeEnderecoEhInvalido(disco[localizacaoINode].inode.enderecoDuploIndireto))
            {
                disco[localizacaoINode].inode.enderecoDuploIndireto = construirINodeIndireto(disco);
            }
            inserirINodeIndiretoDuplo(disco, disco[localizacaoINode].inode.enderecoDuploIndireto, localizacaoINode, quantidadeBlocosFaltantes);
        }

        if (quantidadeBlocosFaltantes > 0)
        {
            if (verificarSeEnderecoEhInvalido(disco[localizacaoINode].inode.enderecoTriploIndireto))
            {
                disco[localizacaoINode].inode.enderecoTriploIndireto = construirINodeIndireto(disco);
            }
            inserirINodeIndiretoTriplo(disco, disco[localizacaoINode].inode.enderecoTriploIndireto, localizacaoINode, quantidadeBlocosFaltantes);
        }

        if (tipoArquivo == TIPO_ARQUIVO_DIRETORIO)
        {
            // caso o tipo de arquivo criado for diretï¿½rio, ï¿½ necessï¿½rio criar dois diretï¿½rios dentro dele, sendo:
            //  "."(Aponta para o inode do diretï¿½rio atual) e
            //  ".."(Aponta para o inode do diretï¿½rio anterior)

            // strcpy(nomeDiretorioAtual, ".");
            // strcpy(nomeDiretorioAnterior, "..");

            // //criaï¿½ï¿½o do diretï¿½rio "." - atual
            // adicionarArquivoNoDiretorio(disco, localizacaoINode, localizacaoINode, nomeDiretorioAtual);

            // //criaÃ§Ã£o do diretï¿½rio ".." - pai
            // addDiretorio(disco, enderecoInodeDiretorioAtual, localizacaoINode, nomeDiretorioAnterior);
            if (verificarSeEnderecoEhInvalido(enderecoInodePai))
                enderecoInodePai = localizacaoINode;

            char nomeDir[MAX_NOME_ARQUIVO];

            strcpy(nomeDir, ".");
            adicionarArquivoNoDiretorio(disco, disco[localizacaoINode].inode.enderecoDireto[0], localizacaoINode, nomeDir);

            strcpy(nomeDir, "..");
            adicionarArquivoNoDiretorio(disco, disco[localizacaoINode].inode.enderecoDireto[0], enderecoInodePai, nomeDir);
        }
        else if (tipoArquivo == TIPO_ARQUIVO_LINK)
        {
            disco[disco[localizacaoINode].inode.enderecoDireto[0]].ls.rota=caminhoLink;
        }
    }

    // retorna endereï¿½o do inode criado
    return localizacaoINode;
}

char verificarSeDiretorioEstaCheia(Diretorio diretorio)
{
    return diretorio.tamanhoLista == DIRETORIO_LIMITE_ARQUIVOS;
}

// adiciona o arquivo dentro da entrada do diretï¿½rio
void adicionarArquivoNoDiretorio(Disco disco[], int enderecoDiretorio, int enderecoInodeCriado, char nomeDiretorioArquivo[MAX_NOME_ARQUIVO])
{

    disco[enderecoDiretorio].diretorio.arquivo[disco[enderecoDiretorio].diretorio.tamanhoLista].localizacaoINode = enderecoInodeCriado;

    strcpy(disco[enderecoDiretorio].diretorio.arquivo[disco[enderecoDiretorio].diretorio.tamanhoLista++].nomenclatura, nomeDiretorioArquivo);
}

void removerArquivoNoDiretorio(Disco disco[], int enderecoEntradaDiretorio, int posicaoRemovida)
{
    for (int i = posicaoRemovida; i < disco[enderecoEntradaDiretorio].diretorio.tamanhoLista; i++)
    {
        disco[enderecoEntradaDiretorio].diretorio.arquivo[i] = disco[enderecoEntradaDiretorio].diretorio.arquivo[i + 1];
    }

    disco[enderecoEntradaDiretorio].diretorio.tamanhoLista--;
}

int procurarArquivoNoDiretorio(Disco disco[], int enderecoEntradaDiretorio, string nomeArquivo){
    int pos=0;

    while (pos < disco[enderecoEntradaDiretorio].diretorio.tamanhoLista)
    {
        if (strcmp(disco[enderecoEntradaDiretorio].diretorio.arquivo[pos].nomenclatura, nomeArquivo.c_str()) == 0)
            break;
        pos++;
    }

    return pos;
}

// funï¿½ï¿½o que busca qual o diretï¿½rio que serï¿½ possï¿½vel inserir o arquivo ou diretï¿½rio
int inserirDiretorioEArquivo(Disco disco[], char tipoArquivo, int enderecoInodeDiretorioAtual, char nomeDiretorioArquivoNovo[MAX_NOME_ARQUIVO], int dimensaoArquivo = 1, string caminhoLink="", int enderecoInodeCriado=obterEnderecoInvalido())
{

    bool consegueInserir = false;
    int i;
    int enderecoInodeDiretorio;
    int enderecoInodeArquivo;
    char privilegio[10];

    enderecoInodeArquivo = verificarExistenciaArquivoOuDiretorio(disco, enderecoInodeDiretorioAtual, nomeDiretorioArquivoNovo);
    if (verificarSeEnderecoEhInvalido(enderecoInodeArquivo))
    {
        int enderecoDireto = disco[enderecoInodeDiretorioAtual].inode.enderecoDireto[0];

        for (i = 0; i < 5 && verificarSeEnderecoEhValido(enderecoDireto) && !consegueInserir; i++)
        {
            enderecoDireto = disco[enderecoInodeDiretorioAtual].inode.enderecoDireto[i];

            if (verificarSeEnderecoEhValido(enderecoDireto) && disco[enderecoDireto].diretorio.tamanhoLista < 10)
            {
                consegueInserir = true;
            }
        }

        // pergunta se nï¿½o conseguiu inserir no diretï¿½rio e finalizou o while antes mesmo de atingir todos os Endereï¿½os.
        if (!consegueInserir && i <= 5 && verificarSeEnderecoEhInvalido(enderecoDireto)) // significa que hï¿½ blocos livres para ser preenchidos e o diretï¿½rio esta cheio.
        {
            enderecoInodeDiretorio = removerBlocoLivreDaLista(disco);
            disco[enderecoInodeDiretorioAtual].inode.enderecoDireto[i - 1] = enderecoInodeDiretorio;
            disco[disco[disco[enderecoInodeDiretorioAtual].inode.enderecoDireto[0]].diretorio.arquivo[0].localizacaoINode].inode.dimensaoArquivo++;
        }
        else if (!consegueInserir) // todos os blocos estï¿½o cheio, consequentemente deve verificar se pode adicionar nos blocos indiretos
        {
            if (verificarSeEnderecoEhInvalido(disco[enderecoInodeDiretorioAtual].inode.enderecoSimplesIndireto))
            {
                disco[enderecoInodeDiretorioAtual].inode.enderecoSimplesIndireto = construirINodeIndireto(disco);
            }
            enderecoInodeDiretorio = procurarNovoBlocoDiretorioLivreEmIndiretoSimples(disco, disco[enderecoInodeDiretorioAtual].inode.enderecoSimplesIndireto, enderecoInodeDiretorioAtual);

            // nï¿½o conseguiu buscar um bloco disponï¿½vel no inode indireto simples
            if (verificarSeEnderecoEhInvalido(enderecoInodeDiretorio))
            {
                if (verificarSeEnderecoEhInvalido(disco[enderecoInodeDiretorioAtual].inode.enderecoDuploIndireto))
                {
                    disco[enderecoInodeDiretorioAtual].inode.enderecoDuploIndireto = construirINodeIndireto(disco);
                }
                enderecoInodeDiretorio = procurarNovoBlocoDiretorioLivreEmIndiretoDuplo(disco, disco[enderecoInodeDiretorioAtual].inode.enderecoDuploIndireto, enderecoInodeDiretorioAtual);

                // nï¿½o conseguiu buscar um bloco disponï¿½vel no inode indireto duplo
                if (verificarSeEnderecoEhInvalido(enderecoInodeDiretorio))
                {
                    if (verificarSeEnderecoEhInvalido(disco[enderecoInodeDiretorioAtual].inode.enderecoTriploIndireto))
                    {
                        disco[enderecoInodeDiretorioAtual].inode.enderecoTriploIndireto = construirINodeIndireto(disco);
                    }

                    enderecoInodeDiretorio = procurarNovoBlocoDiretorioLivreEmIndiretoTriplo(disco, disco[enderecoInodeDiretorioAtual].inode.enderecoTriploIndireto, enderecoInodeDiretorioAtual);
                }
            }
        }
        else // identificou um bloco que pode ser utilizado para inserir o diretï¿½rio
        {
            enderecoInodeDiretorio = enderecoDireto;
        }
        // TODO - DESCOBRIR QUAL O BLOCO LIVRE PARA INSERIR O diretï¿½rio NO diretï¿½rio ATUAL

        if (!enderecoInodeDiretorio)
            return obterEnderecoInvalido();
        else
        {
            if (tipoArquivo == TIPO_ARQUIVO_DIRETORIO)
            {
                converterPermissaoUGOParaTexto(PERMISSAO_PADRAO_DIRETORIO, privilegio, 0);
            }
            else if (tipoArquivo == TIPO_ARQUIVO_ARQUIVO)
            {
                converterPermissaoUGOParaTexto(PERMISSAO_PADRAO_ARQUIVO, privilegio, 0);
            }
            else if (tipoArquivo == TIPO_ARQUIVO_LINK)
            {
                converterPermissaoUGOParaTexto(PERMISSAO_PADRAO_LINKSIMBOLICO, privilegio, 0);
            }

            // adiciona o arquivo dentro da entrada do diretï¿½rio
            if (verificarSeEnderecoEhInvalido(enderecoInodeCriado))
                enderecoInodeCriado = construirINode(disco, tipoArquivo, privilegio, dimensaoArquivo, enderecoInodeDiretorioAtual, caminhoLink);

            adicionarArquivoNoDiretorio(disco, enderecoInodeDiretorio, enderecoInodeCriado, nomeDiretorioArquivoNovo);
        }
        return enderecoInodeDiretorio;
    }

    return obterEnderecoInvalido();
}

int construirDiretorioRaiz(Disco disco[])
{
    char privilegio[10];
    char nomeArquivo[MAX_NOME_ARQUIVO];

    converterPermissaoUGOParaTexto(PERMISSAO_PADRAO_DIRETORIO, privilegio, 0);
    int enderecoInodeDiretorioRaiz = construirINode(disco, TIPO_ARQUIVO_DIRETORIO, privilegio);
    return enderecoInodeDiretorioRaiz;
}

void ordenarRapidamente(int ini, int fim, exibicaoEndereco vet[])
{
    int i = ini;
    int j = fim;
    int troca = true;

    // exibicaoEndereco exibEnd;
    int end;
    char info;

    while (i < j)
    {
        if (troca)
            while (i < j && vet[i].endereco <= vet[j].endereco)
                i++;
        else
            while (i < j && vet[j].endereco >= vet[i].endereco)
                j--;

        end = vet[i].endereco;
        info = vet[i].info;

        vet[i].endereco = vet[j].endereco;
        vet[i].info = vet[j].info;

        vet[j].endereco = end;
        vet[j].info = info;

        // exibEnd = vet[i];
        // vet[i] = vet[j];
        // vet[j] = exibEnd;
        troca = !troca;
    }

    if (ini < i - 1)
        ordenarRapidamente(ini, i - 1, vet);

    if (j + 1 < fim)
        ordenarRapidamente(j + 1, fim, vet);
}

void mostrarDisco(Disco disco[], int tamanhoDisco, int quantidadeBlocosNecessariosListaLivre)
{
    exibicaoEndereco enderecos[tamanhoDisco];
    int TLEnderecos = 0;
    char nomeBlocos[5];
    int quantidadeCasasTamanhoDisco = (int)trunc(log10(tamanhoDisco)) + 1;
    int quantidadeBlocosLivres = obterQuantidadeBlocosDisponveis(disco);

    int linha = 0;
    for (int i = 0; i < tamanhoDisco; i++)
    {
        if (disco[i].bad)
        {
            adicionarEnderecoNaExibicao(enderecos, TLEnderecos, i, 'B');
        }
        // else if(disco[i].diretorio.tamanhoLista > 0){
        //     adicionarEnderecoNaExibicao(enderecos, TLEnderecos, i, 'A');
        // }
        else if (strlen(disco[i].inode.seguranca) > 0)
        {
            // printf("%d - inode\n", i);
            // tem um i-node 
            adicionarEnderecoNaExibicao(enderecos, TLEnderecos, i, 'I');
            for (int j = 0; j < 5 && !verificarSeEnderecoEhInvalido(disco[i].inode.enderecoDireto[j]); j++)
            {
                // printf("%d - arquivo\n", disco[i].inode.enderecoDireto[j]);
                if (!verificarSeEnderecoEhInvalido(disco[i].inode.enderecoDireto[j]) && !verificarSeEnderecoEstaDescalibrado(disco, disco[i].inode.enderecoDireto[j]))
                {
                    if (disco[disco[i].inode.enderecoDireto[j]].ls.rota.length() > 0)
                        adicionarEnderecoNaExibicao(enderecos, TLEnderecos, disco[i].inode.enderecoDireto[j], 'detalhado');
                    else
                        adicionarEnderecoNaExibicao(enderecos, TLEnderecos, disco[i].inode.enderecoDireto[j], 'A');
                }
            }
            
            

            if (!verificarSeEnderecoEhInvalido(disco[i].inode.enderecoSimplesIndireto) && !verificarSeEnderecoEstaDescalibrado(disco, disco[i].inode.enderecoSimplesIndireto))
            {
                int enderecoIndireto = 0;
                enderecoIndireto = disco[i].inode.enderecoSimplesIndireto;
                adicionarEnderecoNaExibicao(enderecos, TLEnderecos, enderecoIndireto, 'I');

                percorrerINodeIndiretoSimples(disco, enderecoIndireto, enderecos, TLEnderecos);
            }

            if (!verificarSeEnderecoEhInvalido(disco[i].inode.enderecoDuploIndireto) && !verificarSeEnderecoEstaDescalibrado(disco, disco[i].inode.enderecoDuploIndireto))
            {
                int enderecoIndireto = disco[i].inode.enderecoDuploIndireto;
                adicionarEnderecoNaExibicao(enderecos, TLEnderecos, enderecoIndireto, 'I');

                percorrerINodeIndiretoDuplo(disco, enderecoIndireto, enderecos, TLEnderecos);
            }

            if (!verificarSeEnderecoEhInvalido(disco[i].inode.enderecoTriploIndireto) && !verificarSeEnderecoEstaDescalibrado(disco, disco[i].inode.enderecoTriploIndireto))
            {
                int enderecoIndireto = disco[i].inode.enderecoTriploIndireto;
                adicionarEnderecoNaExibicao(enderecos, TLEnderecos, enderecoIndireto, 'I');

                percorrerINodeIndiretoTriplo(disco, enderecoIndireto, enderecos, TLEnderecos);
            }
        }
        // else if(disco[i].inodeIndireto.tamanhoLista > 0)
        // {
        //     //ignora o indireto, serï¿½ realizado no inode
        // }
        // else if (strlen(disco[i].ls.rota.c_str()) > 0)
        // {
        //     adicionarEnderecoNaExibicao(enderecos, TLEnderecos, i, 'detalhado');
        // }
        else if (disco[i].lbl.inicio > -1 || (i >= ENDERECO_CABECA_LISTA && i < ENDERECO_CABECA_LISTA + quantidadeBlocosNecessariosListaLivre))
        {

            adicionarEnderecoNaExibicao(enderecos, TLEnderecos, i, 'R');
        }
        else
        {
            // adicionarEnderecoNaExibicao(enderecos, TLEnderecos, i, 'F');
        }
    }

    for (int i = ENDERECO_CABECA_LISTA; i < ENDERECO_CABECA_LISTA + quantidadeBlocosNecessariosListaLivre && disco[i].lbl.inicio > -1; i++)
    {
        for (int j = 0; j <= disco[i].lbl.inicio; j++)
        {
            if (!disco[disco[i].lbl.endereco[j]].bad)
                adicionarEnderecoNaExibicao(enderecos, TLEnderecos, disco[i].lbl.endereco[j], 'F');
        }
    }

    ordenarRapidamente(0, TLEnderecos - 1, enderecos);

    int linhaTela = 1;
    int colunaTela = 1;

    for (int i = 0; i < TLEnderecos; i++)
    {

        // for(int j=0; j<quantidadeCasasTamanhoDisco; j++)
        // {
        //     // printf("%d");
        // }

        printf("[%d %c]\t", enderecos[i].endereco, enderecos[i].info);

        if (linha == 10)
        {
            printf("\n");
            linha = 0;
        }
        linha++;
    }
}

void listarConteudoDiretorio(Disco disco[], int localizacaoINodeAtual, bool exibeDiretorioOculto=false)
{
    int direto, i, linha = 0;

    for (direto = 0; direto < 5 && verificarSeEnderecoEhValido(disco[localizacaoINodeAtual].inode.enderecoDireto[direto]); direto++)
    {
        for (i = (!exibeDiretorioOculto && direto == 0 ? 2 : 0); i < disco[disco[localizacaoINodeAtual].inode.enderecoDireto[direto]].diretorio.tamanhoLista; i++)
        {
            printf("%s\t", disco[disco[localizacaoINodeAtual].inode.enderecoDireto[direto]].diretorio.arquivo[i].nomenclatura);
        }

        if (linha == 10)
        {
            printf("\n");
            linha = 0;
        }
        linha++;
    }

    if (!verificarSeEnderecoEhInvalido(disco[localizacaoINodeAtual].inode.enderecoSimplesIndireto))
    {
        listarDiretorioINodeIndiretoSimples(disco, disco[localizacaoINodeAtual].inode.enderecoSimplesIndireto, linha, 0, exibeDiretorioOculto);
    }

    if (!verificarSeEnderecoEhInvalido(disco[localizacaoINodeAtual].inode.enderecoDuploIndireto))
    {
        listarDiretorioINodeIndiretoDuplo(disco, disco[localizacaoINodeAtual].inode.enderecoDuploIndireto, linha, 0, exibeDiretorioOculto);
    }

    if (!verificarSeEnderecoEhInvalido(disco[localizacaoINodeAtual].inode.enderecoTriploIndireto))
    {
        listarDiretorioINodeIndiretoTriplo(disco, disco[localizacaoINodeAtual].inode.enderecoTriploIndireto, linha, 0, exibeDiretorioOculto);
    }
}

void listarDiretorioDetalhado(Disco disco[], int localizacaoINodeAtual, bool exibeDiretorioOculto=false)
{
    int direto, i, linha = 0, enderecoInodeArquivo;

    for (direto = 0; direto < 5 && verificarSeEnderecoEhValido(disco[localizacaoINodeAtual].inode.enderecoDireto[direto]); direto++)
    {
        for (i = (!exibeDiretorioOculto && direto == 0 ? 2 : 0); i < disco[disco[localizacaoINodeAtual].inode.enderecoDireto[direto]].diretorio.tamanhoLista; i++)
        {
            enderecoInodeArquivo = disco[disco[localizacaoINodeAtual].inode.enderecoDireto[direto]].diretorio.arquivo[i].localizacaoINode;

            printf("%s ", disco[enderecoInodeArquivo].inode.seguranca);
            printf("%d ", disco[enderecoInodeArquivo].inode.quantidadeLinkFisico);
            printf("%s ", obterNomeProprietario(disco[enderecoInodeArquivo].inode.dono).c_str());
            printf("%s ", obterNomeGrupo(disco[enderecoInodeArquivo].inode.categoria).c_str());
            printf("%d ", disco[enderecoInodeArquivo].inode.dimensaoArquivo);
            printf("%s ", disco[enderecoInodeArquivo].inode.momentoUltimaAlteracao);

            if (disco[enderecoInodeArquivo].inode.seguranca[0] == TIPO_ARQUIVO_LINK)
            {
                printf("%s",disco[disco[localizacaoINodeAtual].inode.enderecoDireto[direto]].diretorio.arquivo[i].nomenclatura);
                textcolor(LIGHTBLUE);
                printf(" -> %s\n", 
                                    disco[disco[disco[disco[localizacaoINodeAtual].inode.enderecoDireto[direto]].diretorio.arquivo[i].localizacaoINode].inode.enderecoDireto[0]].ls.rota.c_str());
                textcolor(WHITE);
            }
            else
                printf("%s\n", disco[disco[localizacaoINodeAtual].inode.enderecoDireto[direto]].diretorio.arquivo[i].nomenclatura);
        }
    }

    if (!verificarSeEnderecoEhInvalido(disco[localizacaoINodeAtual].inode.enderecoSimplesIndireto))
    {
        listarDiretorioINodeIndiretoSimples(disco, disco[localizacaoINodeAtual].inode.enderecoSimplesIndireto, linha, 1, exibeDiretorioOculto);
    }

    if (!verificarSeEnderecoEhInvalido(disco[localizacaoINodeAtual].inode.enderecoDuploIndireto))
    {
        listarDiretorioINodeIndiretoDuplo(disco, disco[localizacaoINodeAtual].inode.enderecoDuploIndireto, linha, 1, exibeDiretorioOculto);
    }

    if (!verificarSeEnderecoEhInvalido(disco[localizacaoINodeAtual].inode.enderecoTriploIndireto))
    {
        listarDiretorioINodeIndiretoTriplo(disco, disco[localizacaoINodeAtual].inode.enderecoTriploIndireto, linha, 1, exibeDiretorioOculto);
    }
}

int alterarDiretorio(Disco disco[], int localizacaoINodeAtual, string nomeDiretorio, int localizacaoINodeRaiz, string &rotaAbsoluta)
{
    int direto, i, endereco = -1;

    if (strcmp(nomeDiretorio.c_str(), ".") == 0)
    {
        return disco[disco[localizacaoINodeAtual].inode.enderecoDireto[0]].diretorio.arquivo[0].localizacaoINode;
    }
    else if (strcmp(nomeDiretorio.c_str(), "..") == 0)
    {
        if (localizacaoINodeAtual == localizacaoINodeRaiz)
        {
            rotaAbsoluta.assign("~");
        }
        else
        {
            // encontra a ultima ocorrï¿½ncia de uma string
            size_t pos = rotaAbsoluta.rfind("/");

            // verifica se a posiï¿½ï¿½o encontrada ï¿½ valida
            // se achou uma posicao da string fornecida
            if (pos != std::string::npos)
            {
                // a partir da posicao encontrada remove parte do rota
                rotaAbsoluta.replace(pos, rotaAbsoluta.size(), "");
            }
        }

        return disco[disco[localizacaoINodeAtual].inode.enderecoDireto[0]].diretorio.arquivo[1].localizacaoINode;
    }
    else if (strcmp(nomeDiretorio.c_str(), "/") == 0)
    {
        rotaAbsoluta.assign("~");
        return localizacaoINodeRaiz;
    }
    else if (strcmp(nomeDiretorio.c_str(), "~") == 0)
    {
        rotaAbsoluta.assign("~");
        return localizacaoINodeRaiz;
    }
    else
    {
        if (contarOcorrenciasNaString(nomeDiretorio, '/') > 0)
        {
            int enderecoInodeOrigem = localizacaoINodeAtual;
            vector<string> caminhoOrigem = dividirCaminho(nomeDiretorio);

            for(const auto& str : caminhoOrigem)
            {
                enderecoInodeOrigem = alterarDiretorio(disco, enderecoInodeOrigem, str.c_str(), localizacaoINodeRaiz, rotaAbsoluta); 
            }

            return enderecoInodeOrigem; 
        }
        else
        {
            endereco = verificarExistenciaArquivoOuDiretorio(disco, localizacaoINodeAtual, nomeDiretorio, TIPO_ARQUIVO_ARQUIVO, false);
            if (verificarSeEnderecoEhValido(endereco))
            {
                if (disco[endereco].inode.seguranca[0] == TIPO_ARQUIVO_LINK)
                {
                    int enderecoInodeOrigem = localizacaoINodeAtual;
                    vector<string> caminhoOrigem = dividirCaminho(disco[disco[endereco].inode.enderecoDireto[0]].ls.rota);

                    for(const auto& str : caminhoOrigem)
                    {
                        enderecoInodeOrigem = alterarDiretorio(disco, enderecoInodeOrigem, str.c_str(), localizacaoINodeRaiz, rotaAbsoluta); 
                    }

                    return enderecoInodeOrigem;
                }
                else
                    rotaAbsoluta.append("/" + nomeDiretorio);

                return endereco;
            }

            return localizacaoINodeAtual;
        }
    }
}

bool criarArquivo(Disco disco[], int localizacaoINodeAtual, int localizacaoINodeRaiz, char instrucao[])
{
    string comandoString(instrucao), caminhoAux;
    int endereco = obterEnderecoInvalido();
    vector<string> vetorStringSeparado = dividirString(comandoString, ' ');

    if (vetorStringSeparado.size() >= 1 && vetorStringSeparado.size() <= 2 && obterUltimaPosicao(dividirString(vetorStringSeparado.at(0), '/')).size() <= MAX_NOME_ARQUIVO)
    {
        char nomeArquivo[vetorStringSeparado.at(0).size() + 1];
        strcpy(nomeArquivo, vetorStringSeparado.at(0).c_str());
        int dimensaoArquivo = 0;
        
        if (vetorStringSeparado.size() == 2)
        	dimensaoArquivo = atoi(vetorStringSeparado.at(1).c_str());

        int enderecoInodeOrigem = alterarDiretorio(disco, localizacaoINodeAtual, nomeArquivo, localizacaoINodeRaiz, caminhoAux);
        vetorStringSeparado = dividirCaminho(nomeArquivo);

        endereco = verificarExistenciaArquivoOuDiretorio(disco, enderecoInodeOrigem, obterUltimaPosicao(vetorStringSeparado).c_str());
        if (verificarSeEnderecoEhInvalido(endereco)) // ainda nï¿½o tem nenhum arquivo criado com esse nomenclatura
        {
            int quantidadeBlocosLivres = obterQuantidadeBlocosDisponveis(disco);
            int quantidadeBlocosUsados = obterQuantidadeBlocosUtilizar(disco, ceil((float)dimensaoArquivo / (float)10));            
            strcpy(nomeArquivo, obterUltimaPosicao(vetorStringSeparado).c_str());
            if (quantidadeBlocosLivres - quantidadeBlocosUsados >= 0)
                return verificarSeEnderecoEhValido(inserirDiretorioEArquivo(disco, TIPO_ARQUIVO_ARQUIVO, enderecoInodeOrigem, nomeArquivo, dimensaoArquivo));
        }
    }

    return false;
}

void procurarBlocosDisponiveisEOcupados(Disco disco[], int &qtdBlocosLivres, int &qtdBlocosOcupados, int quantidadeBlocosTotais, int quantidadeBlocosNecessariosListaLivre)
{

    for (int i = 0; i < quantidadeBlocosTotais; i++)
    {
        if (disco[i].bad)
        {
            qtdBlocosOcupados++;
        }
        // else if(disco[i].diretorio.tamanhoLista > 0){
        //     adicionarEnderecoNaExibicao(enderecos, TLEnderecos, i, 'A');
        // }
        else if (strlen(disco[i].inode.seguranca) > 0)
        {
            qtdBlocosOcupados++;
            for (int j = 0; j < 5 && !verificarSeEnderecoEhInvalido(disco[i].inode.enderecoDireto[j]); j++)
            {
                // printf("%d - arquivo\n", disco[i].inode.enderecoDireto[j]);
                if (!verificarSeEnderecoEhInvalido(disco[i].inode.enderecoDireto[j]))
                    qtdBlocosOcupados++;
            }

            if (!verificarSeEnderecoEhInvalido(disco[i].inode.enderecoSimplesIndireto))
            {
                int enderecoIndireto = 0;
                enderecoIndireto = disco[i].inode.enderecoSimplesIndireto;
                // adicionarEnderecoNaExibicao(enderecos, TLEnderecos, enderecoIndireto, 'I');

                procurarOcupacaoInodeIndiretoSimples(disco, enderecoIndireto, qtdBlocosOcupados);
            }

            if (!verificarSeEnderecoEhInvalido(disco[i].inode.enderecoDuploIndireto))
            {
                int enderecoIndireto = disco[i].inode.enderecoDuploIndireto;
                // adicionarEnderecoNaExibicao(enderecos, TLEnderecos, enderecoIndireto, 'I');

                procurarOcupacaoInodeIndiretoDuplo(disco, enderecoIndireto, qtdBlocosOcupados);
            }

            if (!verificarSeEnderecoEhInvalido(disco[i].inode.enderecoTriploIndireto))
            {
                int enderecoIndireto = disco[i].inode.enderecoTriploIndireto;
                // adicionarEnderecoNaExibicao(enderecos, TLEnderecos, enderecoIndireto, 'I');

                procurarOcupacaoInodeIndiretoTriplo(disco, enderecoIndireto, qtdBlocosOcupados);
            }
        }

        else if (strlen(disco[i].ls.rota.c_str()) > 0)
        {
            qtdBlocosOcupados++;
        }
        else if (disco[i].lbl.inicio > -1 || (i >= ENDERECO_CABECA_LISTA && i < ENDERECO_CABECA_LISTA + quantidadeBlocosNecessariosListaLivre))
        {

            qtdBlocosOcupados++;
        }
    }

    for (int i = ENDERECO_CABECA_LISTA; i < ENDERECO_CABECA_LISTA + quantidadeBlocosNecessariosListaLivre && disco[i].lbl.inicio > -1; i++)
    {
        for (int j = 0; j <= disco[i].lbl.inicio; j++)
        {
            if (!disco[disco[i].lbl.endereco[j]].bad)
                qtdBlocosLivres++;
        }
    }
}

void abrirArquivoParaEdicao(Disco disco[], int localizacaoINodeAtual, string nomeArquivo, string &enderecosUtilizados)
{
    int enderecoInodeArquivo = obterEnderecoInvalido(), i, bad = 0;
    char enderecoToChar[100];
    bool primeiraVez = enderecosUtilizados.size() == 0;

    // primeira vez que entra
    if (primeiraVez)
        enderecoInodeArquivo = verificarExistenciaArquivoOuDiretorio(disco, localizacaoINodeAtual, nomeArquivo, TIPO_ARQUIVO_ARQUIVO);
    else // demais extensï¿½es de inode
        enderecoInodeArquivo = localizacaoINodeAtual;

    if (verificarSeEnderecoEhValido(enderecoInodeArquivo))
    {
        if (primeiraVez)
            printf("%s: ", nomeArquivo.c_str());

        itoa(enderecoInodeArquivo, enderecoToChar, 10);
        enderecosUtilizados.append(enderecoToChar).append("-");

        for (int direto = 0; direto < 5; direto++)
        {
            if (verificarSeEnderecoEhValido(disco[enderecoInodeArquivo].inode.enderecoDireto[direto]))
            {
                itoa(disco[enderecoInodeArquivo].inode.enderecoDireto[direto], enderecoToChar, 10);
                enderecosUtilizados.append(enderecoToChar).append("-");
            }
        }

        if (!verificarSeEnderecoEhInvalido(disco[enderecoInodeArquivo].inode.enderecoSimplesIndireto))
        {
            itoa(disco[enderecoInodeArquivo].inode.enderecoSimplesIndireto, enderecoToChar, 10);
            enderecosUtilizados.append(enderecoToChar).append("-");

            retornarEnderecosINodeIndiretoSimples(disco, disco[enderecoInodeArquivo].inode.enderecoSimplesIndireto, enderecosUtilizados);
        }

        if (!verificarSeEnderecoEhInvalido(disco[enderecoInodeArquivo].inode.enderecoDuploIndireto))
        {
            itoa(disco[enderecoInodeArquivo].inode.enderecoDuploIndireto, enderecoToChar, 10);
            enderecosUtilizados.append(enderecoToChar).append("-");

            retornarEnderecosINodeIndiretoDuplo(disco, disco[enderecoInodeArquivo].inode.enderecoDuploIndireto, enderecosUtilizados);
        }

        if (!verificarSeEnderecoEhInvalido(disco[enderecoInodeArquivo].inode.enderecoTriploIndireto))
        {
            itoa(disco[enderecoInodeArquivo].inode.enderecoTriploIndireto, enderecoToChar, 10);
            enderecosUtilizados.append(enderecoToChar).append("-");

            retornarEnderecosINodeIndiretoTriplo(disco, disco[enderecoInodeArquivo].inode.enderecoTriploIndireto, enderecosUtilizados);
        }

        if (primeiraVez)
        {
            int qtdBlocos = 0;
            for (i = 0; i < enderecosUtilizados.size(); i++)
            {
                if (enderecosUtilizados.at(i) == '-')
                {
                    qtdBlocos++;
                }
            }
            
            // separa em partes usando '-'
		    vector<string> partes = dividirString(enderecosUtilizados, '-');
		
		    // converte pra nï¿½meros
		    vector<int> enderecos;
		    for (const string &p : partes) {
		        stringstream ss(p);
			    int num;
			    ss >> num;
			    enderecos.push_back(num);
			    
   			}
   			
   			for (int n : enderecos) {
   				if(disco[n].bad == 1)
		        	bad = 1;
		    }
    		
    		if(bad == 0){
	            printf("%s\n", enderecosUtilizados.substr(0, enderecosUtilizados.size() - 1).c_str());
	            printf("Quantidade de enderecos: %d", qtdBlocos);
        	}
			else
				printf("Arquivo corrompido");	
        }
    }
    else
    {
        if (primeiraVez)
            printf("Arquivo nao encontrado!");
    }

    if (primeiraVez)
        printf("\n");
}


bool deletarArquivo(Disco disco[], int localizacaoINodeAtual, string nomeArquivo, int primeiraVez = 1)
{
    int enderecoInodeArquivo, endereco;

    if (primeiraVez)
        enderecoInodeArquivo = verificarExistenciaArquivoOuDiretorio(disco, localizacaoINodeAtual, nomeArquivo, TIPO_ARQUIVO_ARQUIVO);
    else // demais extensï¿½es de inode
        enderecoInodeArquivo = localizacaoINodeAtual;

    if (verificarSeEnderecoEhValido(enderecoInodeArquivo))
    {
        if (primeiraVez)
        {
            // remover da entrada do diretï¿½rio o nomenclatura do arquivo.
            int enderecoEntradaDiretorio = procurarEnderecoEntradaDiretorioArquivo(disco, localizacaoINodeAtual, nomeArquivo, TIPO_ARQUIVO_ARQUIVO);
            int pos = 0;
            
			pos = procurarArquivoNoDiretorio(disco, enderecoEntradaDiretorio, nomeArquivo);
            removerArquivoNoDiretorio(disco, enderecoEntradaDiretorio, pos);
            

            if (--disco[enderecoInodeArquivo].inode.quantidadeLinkFisico == 0)
            {
            	for (int direto = 0; direto < 5; direto++)
        		{
            		if (verificarSeEnderecoEhValido(disco[enderecoInodeArquivo].inode.enderecoDireto[direto]))
           		 	{
		                inicializarDiscoRestrito(disco[disco[enderecoInodeArquivo].inode.enderecoDireto[direto]]);
		                adicionarBlocoLivreNaLista(disco, disco[enderecoInodeArquivo].inode.enderecoDireto[direto]);
		                disco[enderecoInodeArquivo].inode.enderecoDireto[direto] = obterEnderecoInvalido();
		            }
		        }
		
		        if (!verificarSeEnderecoEhInvalido(disco[enderecoInodeArquivo].inode.enderecoSimplesIndireto))
		        {
		            removerEnderecosINodeIndiretoSimples(disco, disco[enderecoInodeArquivo].inode.enderecoSimplesIndireto);
		
		            inicializarDiscoRestrito(disco[disco[enderecoInodeArquivo].inode.enderecoSimplesIndireto]);
		            adicionarBlocoLivreNaLista(disco, disco[enderecoInodeArquivo].inode.enderecoSimplesIndireto);
		
		            disco[enderecoInodeArquivo].inode.enderecoSimplesIndireto = obterEnderecoInvalido();
		        }
		
		        if (!verificarSeEnderecoEhInvalido(disco[enderecoInodeArquivo].inode.enderecoDuploIndireto))
		        {
		            removerEnderecosINodeIndiretoDuplo(disco, disco[enderecoInodeArquivo].inode.enderecoDuploIndireto);
		
		            inicializarDiscoRestrito(disco[disco[enderecoInodeArquivo].inode.enderecoDuploIndireto]);
		            adicionarBlocoLivreNaLista(disco, disco[enderecoInodeArquivo].inode.enderecoDuploIndireto);
		
		            disco[enderecoInodeArquivo].inode.enderecoDuploIndireto = obterEnderecoInvalido();
		        }
		
		        if (!verificarSeEnderecoEhInvalido(disco[enderecoInodeArquivo].inode.enderecoTriploIndireto))
		        {
		            removerEnderecosINodeIndiretoTriplo(disco, disco[enderecoInodeArquivo].inode.enderecoTriploIndireto);
		
		            inicializarDiscoRestrito(disco[disco[enderecoInodeArquivo].inode.enderecoTriploIndireto]);
		            adicionarBlocoLivreNaLista(disco, disco[enderecoInodeArquivo].inode.enderecoTriploIndireto);
		
		            disco[enderecoInodeArquivo].inode.enderecoTriploIndireto = obterEnderecoInvalido();
		        }
		        
		        inicializarDiscoRestrito(disco[enderecoInodeArquivo]);
		        adicionarBlocoLivreNaLista(disco, enderecoInodeArquivo);
            }

            return true;
        }
        else
        {
        	// Quando for recursï¿½o para as extensï¿½es de I-node (!primeiraVez)
            // Precisamos apagar os blocos vinculados ï¿½ extensï¿½o
            for (int direto = 0; direto < 5; direto++)
            {
                if (verificarSeEnderecoEhValido(disco[enderecoInodeArquivo].inode.enderecoDireto[direto]))
                {
                    inicializarDiscoRestrito(disco[disco[enderecoInodeArquivo].inode.enderecoDireto[direto]]);
                    adicionarBlocoLivreNaLista(disco, disco[enderecoInodeArquivo].inode.enderecoDireto[direto]);
                    disco[enderecoInodeArquivo].inode.enderecoDireto[direto] = obterEnderecoInvalido();
                }
            }
            // (Para garantir estabilidade, simplificamos o else focado apenas nos diretos caso o professor use extensï¿½es)
            
            inicializarDiscoRestrito(disco[enderecoInodeArquivo]);
            adicionarBlocoLivreNaLista(disco, enderecoInodeArquivo);
            return true;
        }
    }

    return false;
}

bool deletarDiretorio(Disco disco[], int localizacaoINodeAtual, string nomeDiretorio, int &contadorDiretorio, int primeiraVez = 1)
{    
    int enderecoEntradaDiretorio, pos = 0;

    if (primeiraVez)        
        enderecoEntradaDiretorio = procurarEnderecoEntradaDiretorioArquivo(disco, localizacaoINodeAtual, nomeDiretorio, TIPO_ARQUIVO_DIRETORIO);
    else
        enderecoEntradaDiretorio = localizacaoINodeAtual;

    // printf("\n [%d] \n", enderecoEntradaDiretorio);
    if (verificarSeEnderecoEhValido(enderecoEntradaDiretorio))
    {
        int enderecoInodeDiretorio;
        if(primeiraVez)
        {
            pos = procurarArquivoNoDiretorio(disco, enderecoEntradaDiretorio, nomeDiretorio);

            enderecoInodeDiretorio = disco[enderecoEntradaDiretorio].diretorio.arquivo[pos].localizacaoINode;
        }
        else
            enderecoInodeDiretorio = localizacaoINodeAtual;
        
        for (int i = 0; i < 5; i++)
        {
            if (verificarSeEnderecoEhValido(disco[enderecoInodeDiretorio].inode.enderecoDireto[i]))
            {
                contadorDiretorio += disco[disco[enderecoInodeDiretorio].inode.enderecoDireto[i]].diretorio.tamanhoLista;
            }
        }

        if (verificarSeEnderecoEhValido(disco[enderecoInodeDiretorio].inode.enderecoSimplesIndireto))
            retornarQuantidadeDiretorioINodeIndiretoSimples(disco, disco[enderecoInodeDiretorio].inode.enderecoSimplesIndireto, contadorDiretorio);
        if (verificarSeEnderecoEhValido(disco[enderecoInodeDiretorio].inode.enderecoDuploIndireto))
            retornarQuantidadeDiretorioINodeIndiretoDuplo(disco, disco[enderecoInodeDiretorio].inode.enderecoDuploIndireto, contadorDiretorio);
        if (verificarSeEnderecoEhValido(disco[enderecoInodeDiretorio].inode.enderecoTriploIndireto))
            retornarQuantidadeDiretorioINodeIndiretoTriplo(disco, disco[enderecoInodeDiretorio].inode.enderecoTriploIndireto, contadorDiretorio);

        if(primeiraVez)
        {
            // printf("\n-- %d --\n", contadorDiretorio);
            if (contadorDiretorio <= 2)
            {
                if (--disco[enderecoInodeDiretorio].inode.quantidadeLinkFisico == 0)
                {
                    if (verificarSeEnderecoEhValido(disco[enderecoInodeDiretorio].inode.enderecoDireto[0]))
                    {
                        inicializarDiscoRestrito(disco[disco[enderecoInodeDiretorio].inode.enderecoDireto[0]]);
                        adicionarBlocoLivreNaLista(disco, disco[enderecoInodeDiretorio].inode.enderecoDireto[0]);
                        disco[enderecoInodeDiretorio].inode.enderecoDireto[0] = obterEnderecoInvalido();
                    }

                    inicializarDiscoRestrito(disco[enderecoInodeDiretorio]);
                    adicionarBlocoLivreNaLista(disco, enderecoInodeDiretorio);
                }

                removerArquivoNoDiretorio(disco, enderecoEntradaDiretorio, pos);

                return true;
            }
        }
    }

    return false;
}

void criarLinkSimbolico(Disco disco[], int localizacaoINodeAtual, string instrucao, int localizacaoINodeRaiz)
{
    char caminhoDestinoChar[300];
    string caminhoExemplo;
    string caminhoAux;
    vector<string> caminhosOrigemDestino, caminhoOrigem, caminhoDestino;

    caminhosOrigemDestino = dividirString(instrucao, ' ');

    if (caminhosOrigemDestino.size() == 2)
    {
        caminhoOrigem = dividirString(caminhosOrigemDestino.at(0), '/');
        caminhoDestino = dividirString(caminhosOrigemDestino.at(1), '/');
    	
    	int enderecoInodeOrigem = localizacaoINodeAtual;
        string tempCaminhoAux;
        
        bool permissaoConcedida = true;
        
        for(const auto& str : caminhoOrigem)
        {
            enderecoInodeOrigem = alterarDiretorio(disco, enderecoInodeOrigem, str.c_str(), localizacaoINodeRaiz, tempCaminhoAux);
        }

        if (verificarSeEnderecoEhValido(enderecoInodeOrigem))
        {
            string nomeDiretorioOrigem = obterUltimaPosicao(caminhoOrigem);
            int enderecoEntradaDiretorio = procurarEnderecoEntradaDiretorioArquivo(disco, enderecoInodeOrigem, nomeDiretorioOrigem);
            
            if (!verificarSeEnderecoEhInvalido(enderecoEntradaDiretorio))
            {
                int pos = procurarArquivoNoDiretorio(disco, enderecoEntradaDiretorio, nomeDiretorioOrigem);
                int inodeAlvo = disco[enderecoEntradaDiretorio].diretorio.arquivo[pos].localizacaoINode;

                if (disco[inodeAlvo].inode.seguranca[1] == '-')
                	permissaoConcedida = false;
            }
        }
        
        if (!permissaoConcedida)
        {
                    textcolor(RED);
                    printf("privilegio negada!\n");
                    textcolor(WHITE);
        }
        else
        {
        	strcpy(caminhoDestinoChar, caminhoDestino.at(0).c_str());
	        caminhoAux = caminhosOrigemDestino.at(0);
	        inserirDiretorioEArquivo(disco, TIPO_ARQUIVO_LINK, localizacaoINodeAtual, caminhoDestinoChar, 1, caminhoAux);
        }
    }
}

void criarLinkFisico(Disco disco[], int localizacaoINodeAtual, string instrucao, int localizacaoINodeRaiz)
{
    int pos=0;
    char caminhoDestinoChar[300];
    string caminhoExemplo, nomeDiretorioOrigem, nomeDiretorioDestino;
    string caminhoAux;
    vector<string> caminhosOrigemDestino, caminhoOrigem, caminhoDestino;
    
    caminhosOrigemDestino = dividirString(instrucao, ' ');

    if (caminhosOrigemDestino.size() == 2)
    {
        int enderecoInodeOrigem = localizacaoINodeAtual, enderecoEntradaDiretorio,
        enderecoInodeDestino = localizacaoINodeAtual;

        caminhoOrigem = dividirString(caminhosOrigemDestino.at(0), '/');
        caminhoDestino = dividirString(caminhosOrigemDestino.at(1), '/');
		
		bool permissaoConcedida = true;
        
        for(const auto& str : caminhoOrigem)
        {
            enderecoInodeOrigem = alterarDiretorio(disco, enderecoInodeOrigem, str.c_str(), localizacaoINodeRaiz, caminhoAux);
        }

        if (verificarSeEnderecoEhValido(enderecoInodeOrigem))
        {
            nomeDiretorioOrigem.assign(obterUltimaPosicao(caminhoOrigem));
            enderecoEntradaDiretorio = procurarEnderecoEntradaDiretorioArquivo(disco, enderecoInodeOrigem, nomeDiretorioOrigem);
            
			if (verificarSeEnderecoEhInvalido(enderecoEntradaDiretorio))
            {
            	if (disco[enderecoInodeOrigem].inode.seguranca[1] == '-')
            		permissaoConcedida = false;
            	else
                	disco[enderecoInodeOrigem].inode.quantidadeLinkFisico++;
            }
            else
            {
                pos = procurarArquivoNoDiretorio(disco, enderecoEntradaDiretorio, nomeDiretorioOrigem);
                enderecoInodeOrigem = disco[enderecoEntradaDiretorio].diretorio.arquivo[pos].localizacaoINode;
				
				if (disco[enderecoInodeOrigem].inode.seguranca[1] == '-')
            		permissaoConcedida = false;
            	else
                	disco[enderecoInodeOrigem].inode.quantidadeLinkFisico++;
            }
            
            if (!permissaoConcedida)
            {
            	textcolor(RED);
                printf("privilegio negada!\n");
                textcolor(WHITE);
            }
            else
            {
            	for(const auto& str : caminhoDestino)
	            {
	                enderecoInodeDestino = alterarDiretorio(disco, enderecoInodeDestino, str.c_str(), localizacaoINodeRaiz, caminhoAux);
	            }
	
	            nomeDiretorioDestino.assign(caminhoDestino.at(caminhoDestino.size()-1));
	            strcpy(caminhoDestinoChar, nomeDiretorioDestino.c_str());
	
	            inserirDiretorioEArquivo(disco, disco[enderecoInodeOrigem].inode.seguranca[0], 
	                                enderecoInodeDestino, caminhoDestinoChar, 1, caminhoAux, 
	                                enderecoInodeOrigem);
            }
        }
    }
}

void removerLinkSimbolico(Disco disco[], int localizacaoINodeAtual, string nomeArquivo, int localizacaoINodeRaiz)
{
    int enderecoEntradaDiretorio = procurarEnderecoEntradaDiretorioArquivo(disco, localizacaoINodeAtual, nomeArquivo),
        pos = 0;

    if (verificarSeEnderecoEhValido(enderecoEntradaDiretorio))
    {
        pos = procurarArquivoNoDiretorio(disco, enderecoEntradaDiretorio, nomeArquivo);

        if (disco[disco[enderecoEntradaDiretorio].diretorio.arquivo[pos].localizacaoINode].inode.seguranca[0] == TIPO_ARQUIVO_LINK)
        {
            disco[disco[enderecoEntradaDiretorio].diretorio.arquivo[pos].localizacaoINode].inode.enderecoDireto[0];

            //inicializa aquela posiï¿½ï¿½o do disco para remover o apontamento
            inicializarDisco(disco[disco[disco[enderecoEntradaDiretorio].diretorio.arquivo[pos].localizacaoINode].inode.enderecoDireto[0]]);
            //joga de volta pra lista de blocos livres
            adicionarBlocoLivreNaLista(disco, disco[disco[enderecoEntradaDiretorio].diretorio.arquivo[pos].localizacaoINode].inode.enderecoDireto[0]);

            //inicializa a posiï¿½ï¿½o do disco que armazena o inode
            inicializarDisco(disco[disco[enderecoEntradaDiretorio].diretorio.arquivo[pos].localizacaoINode]);
            //joga de volta pra lista de blocos livres
            adicionarBlocoLivreNaLista(disco, disco[disco[enderecoEntradaDiretorio].diretorio.arquivo[pos].localizacaoINode].inode.enderecoDireto[0]);


            removerArquivoNoDiretorio(disco, enderecoEntradaDiretorio, pos);
        }
    }
}

bool removerLinkFisico(Disco disco[], int localizacaoINodeAtual, string nomeArquivo, int localizacaoINodeRaiz){
    int enderecoEntradaDiretorio = procurarEnderecoEntradaDiretorioArquivo(disco, localizacaoINodeAtual, nomeArquivo),
        pos = 0;

    if (verificarSeEnderecoEhValido(enderecoEntradaDiretorio))
    {
        pos = procurarArquivoNoDiretorio(disco, enderecoEntradaDiretorio, nomeArquivo);

        if (disco[disco[enderecoEntradaDiretorio].diretorio.arquivo[pos].localizacaoINode].inode.seguranca[0] == TIPO_ARQUIVO_ARQUIVO)
        {
            return deletarArquivo(disco, localizacaoINodeAtual, nomeArquivo);
        }
        else if (disco[disco[enderecoEntradaDiretorio].diretorio.arquivo[pos].localizacaoINode].inode.seguranca[0] == TIPO_ARQUIVO_DIRETORIO)
        {
            int contadorDiretorio=0;
            return deletarDiretorio(disco, localizacaoINodeAtual, nomeArquivo, contadorDiretorio);
        }
    }

    return false;
}

void alterarPermissao(char privilegio[], char tipoUGO, bool adiciona, char permFornecida[])
{
    int pos;

    if (tipoUGO == 'u')
        pos=1;
    else if (tipoUGO == 'g')
        pos=4;
    else
        pos=7;

    if (adiciona)
    {
        //se permFornecida for diferente de '-', significa que estou adicionando
        if (permFornecida[0] != '-')
            privilegio[pos] = permFornecida[0];

        pos++;
        if (permFornecida[1] != '-')
            privilegio[pos] = permFornecida[1];

        pos++;
        if (permFornecida[2] != '-')
            privilegio[pos] = permFornecida[2];
    }
    else
    {
        //se permFornecida for igual a '-', significa que estou removendo
        if (permFornecida[0] == '-')
            privilegio[pos] = permFornecida[0];

        pos++;
        if (permFornecida[1] == '-')
            privilegio[pos] = permFornecida[1];

        pos++;
        if (permFornecida[2] == '-')
            privilegio[pos] = permFornecida[2];
    }
}

void definirPermissao(Disco disco[], int localizacaoINodeAtual, string instrucao)
{
    char perm[4];
    int r, w, x, pos;
    char privilegio[11];
    vector<string> dividirComando = dividirString(instrucao, ' ');
    //chmod u+w teste.old
    //chmod g+rw teste.old
    //chmod o-rwx teste.old
    //chmod
    //chmod 777 teste.old

    if (dividirComando.size() == 2)
    {
        string permissaoSolicitada(converterParaMinusculas(dividirComando.at(0)));
        string nomeArquivo(dividirComando.at(1));
        int localizacaoINode = verificarExistenciaArquivoOuDiretorio(disco, localizacaoINodeAtual, nomeArquivo);

        if (verificarSeEnderecoEhValido(localizacaoINode))
        {
            strcpy(privilegio, disco[localizacaoINode].inode.seguranca);

            if (permissaoSolicitada.at(1) == '+') //estï¿½ adicionando permissï¿½o
            {
                r = contarOcorrenciasNaString(permissaoSolicitada.substr(2), 'r');
                w = contarOcorrenciasNaString(permissaoSolicitada.substr(2), 'w');
                x = contarOcorrenciasNaString(permissaoSolicitada.substr(2), 'x');

                if (r > 0)
                    perm[0] = 'r';
                else
                    perm[0] = '-';

                if (w > 0)
                    perm[1] = 'w';
                else
                    perm[1] = '-';

                if (x > 0)
                    perm[2] = 'x';
                else
                    perm[2] = '-';

                perm[3] = '\0';

                switch(tolower(permissaoSolicitada.at(0)))
                {
                    case 'u':
                    case 'g':
                    case 'o':
                        alterarPermissao(privilegio, permissaoSolicitada.at(0), true, perm);
                    break;

                    case 'a':
                        alterarPermissao(privilegio, 'u', true, perm);
                        alterarPermissao(privilegio, 'g', true, perm);
                        alterarPermissao(privilegio, 'o', true, perm);
                    break;
                }

                strcpy(disco[localizacaoINode].inode.seguranca, privilegio);
            }
            else if (permissaoSolicitada.at(1) == '-') //estï¿½ retirando permissï¿½o
            {
                r = contarOcorrenciasNaString(permissaoSolicitada.substr(2), 'r');
                w = contarOcorrenciasNaString(permissaoSolicitada.substr(2), 'w');
                x = contarOcorrenciasNaString(permissaoSolicitada.substr(2), 'x');

                if (r > 0)
                    perm[0] = '-';
                else
                    perm[0] = 'r';

                if (w > 0)
                    perm[1] = '-';
                else
                    perm[1] = 'w';

                if (x > 0)
                    perm[2] = '-';
                else
                    perm[2] = 'x';

                perm[3] = '\0';

                switch(tolower(permissaoSolicitada.at(0)))
                {
                    case 'u':
                    case 'g':
                    case 'o':
                        alterarPermissao(privilegio, permissaoSolicitada.at(0), false, perm);
                    break;

                    case 'a':
                        alterarPermissao(privilegio, 'u', false, perm);
                        alterarPermissao(privilegio, 'g', false, perm);
                        alterarPermissao(privilegio, 'o', false, perm);
                    break;
                }

                strcpy(disco[localizacaoINode].inode.seguranca, privilegio);
            }
            else //utilizou ou caracter ou int
            { 
                if (permissaoSolicitada.at(0) >= '0' && permissaoSolicitada.at(0) <= '7')
                {
                    int per = atoi(permissaoSolicitada.c_str());
                    converterPermissaoUGOParaTexto(per, privilegio, 1);

                    strcpy(disco[localizacaoINode].inode.seguranca, privilegio);
                }
            }
        }
    }
}

// utilizado para exibir os blocos do arquivo
void buscaBlocosInodeIndiretoSimples(Disco disco[], int enderecoInodeIndireto, string &enderecosUtilizados, int ChamadoPeloTriplo = 0)
{
    char enderecoToChar[300];
    int inicio = 0;
    // adiciona novos blocos enquanto a quantidade for menor que o necessï¿½rio e que nï¿½o esteja cheio
    while (inicio < disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista && inicio < MAX_INODEINDIRETO - ChamadoPeloTriplo)
    {
        itoa(disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio], enderecoToChar, 10);
        enderecosUtilizados.append(enderecoToChar).append("-");

        inicio++;
    }

    if (ChamadoPeloTriplo)
    {
        if (verificarSeEnderecoEhValido(disco[enderecoInodeIndireto].inodeIndireto.endereco[MAX_INODEINDIRETO - 1]))
        {
            string nomeArq;
            nomeArq.assign("");

            procurarBlocosDoArquivo(disco, disco[enderecoInodeIndireto].inodeIndireto.endereco[MAX_INODEINDIRETO - 1], enderecosUtilizados);
        }
    }
}

// utilizado para exibir os blocos do arquivo
void buscaBlocosInodeIndiretoDuplo(Disco disco[], int enderecoInodeIndireto, string &enderecosUtilizados, int ChamadoPeloTriplo = 0)
{
    char enderecoToChar[300];
    int inicio = 0;
    // adiciona novos blocos enquanto a quantidade for menor que o necessï¿½rio e que nï¿½o esteja cheio
    while (inicio < disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista && inicio < MAX_INODEINDIRETO)
    {
        itoa(disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio], enderecoToChar, 10);
        enderecosUtilizados.append(enderecoToChar).append("-");

        buscaBlocosInodeIndiretoSimples(disco, disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio], enderecosUtilizados, ChamadoPeloTriplo && inicio + 1 == MAX_INODEINDIRETO);
        inicio++;
    }
}

// utilizado para exibir os blocos do arquivo
void buscaBlocosInodeIndiretoTriplo(Disco disco[], int enderecoInodeIndireto, string &enderecosUtilizados)
{
    char enderecoToChar[300];
    int inicio = 0;
    // adiciona novos blocos enquanto a quantidade for menor que o necessï¿½rio e que nï¿½o esteja cheio
    while (inicio < disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista && inicio < MAX_INODEINDIRETO)
    {
        itoa(disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio], enderecoToChar, 10);
        enderecosUtilizados.append(enderecoToChar).append("-");

        buscaBlocosInodeIndiretoDuplo(disco, disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio], enderecosUtilizados, 1);
        inicio++;
    }
}

void procurarBlocosDoArquivo(Disco disco[], int enderecoInodeArquivo, string &enderecosUtilizados)
{
    int i;
    char enderecoToChar[100];
    bool primeiraVez = enderecosUtilizados.size() == 0;

    if (verificarSeEnderecoEhValido(enderecoInodeArquivo))
    {
        itoa(enderecoInodeArquivo, enderecoToChar, 10);
        enderecosUtilizados.append(enderecoToChar).append("-");

        for (int direto = 0; direto < 5; direto++)
        {
            if (verificarSeEnderecoEhValido(disco[enderecoInodeArquivo].inode.enderecoDireto[direto]))
            {
                itoa(disco[enderecoInodeArquivo].inode.enderecoDireto[direto], enderecoToChar, 10);
                enderecosUtilizados.append(enderecoToChar).append("-");
            }
        }

        if (!verificarSeEnderecoEhInvalido(disco[enderecoInodeArquivo].inode.enderecoSimplesIndireto))
        {
            itoa(disco[enderecoInodeArquivo].inode.enderecoSimplesIndireto, enderecoToChar, 10);
            enderecosUtilizados.append(enderecoToChar).append("-");

            buscaBlocosInodeIndiretoSimples(disco, disco[enderecoInodeArquivo].inode.enderecoSimplesIndireto, enderecosUtilizados);
        }

        if (!verificarSeEnderecoEhInvalido(disco[enderecoInodeArquivo].inode.enderecoDuploIndireto))
        {
            itoa(disco[enderecoInodeArquivo].inode.enderecoDuploIndireto, enderecoToChar, 10);
            enderecosUtilizados.append(enderecoToChar).append("-");

            buscaBlocosInodeIndiretoDuplo(disco, disco[enderecoInodeArquivo].inode.enderecoDuploIndireto, enderecosUtilizados);
        }

        if (!verificarSeEnderecoEhInvalido(disco[enderecoInodeArquivo].inode.enderecoTriploIndireto))
        {
            itoa(disco[enderecoInodeArquivo].inode.enderecoTriploIndireto, enderecoToChar, 10);
            enderecosUtilizados.append(enderecoToChar).append("-");

            buscaBlocosInodeIndiretoTriplo(disco, disco[enderecoInodeArquivo].inode.enderecoTriploIndireto, enderecosUtilizados);
        }

        if (primeiraVez)
        {
            enderecosUtilizados = enderecosUtilizados.substr(0, enderecosUtilizados.size() - 1);
        }
    }
}



// utilizado para exibir os diretï¿½rios do disco
void listaDiretorioAtualIgualExplorerInodeIndiretoSimples(Disco disco[], int enderecoInodeIndireto, int ChamadoPeloTriplo = 0)
{
    int enderecoInodeArquivo;
    int i;

    string blocos;

    int inicio = 0;
    // adiciona novos blocos enquanto a quantidade for menor que o necessï¿½rio e que nï¿½o esteja cheio
    while (inicio < disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista && inicio < MAX_INODEINDIRETO - ChamadoPeloTriplo)
    {

        for (i = 0; i < disco[disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio]].diretorio.tamanhoLista; i++)
        {
            enderecoInodeArquivo = disco[disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio]].diretorio.arquivo[i].localizacaoINode;

            printf("%s\t\t",disco[enderecoInodeArquivo].diretorio.arquivo[i].nomenclatura);

            if (disco[enderecoInodeArquivo].inode.seguranca[0] == TIPO_ARQUIVO_LINK)
            {
                printf("Link\t\t");
            }
            else if (disco[enderecoInodeArquivo].inode.seguranca[0] == TIPO_ARQUIVO_DIRETORIO)
            {
                printf("Diretorio\t");
            }
            else
            {
                printf("Arquivo\t\t");
            }

            blocos.assign("");
            procurarBlocosDoArquivo(disco, enderecoInodeArquivo, blocos);
            printf("%s\n", blocos.c_str());
        }
    
        inicio++;
    }

    if (ChamadoPeloTriplo)
    {
        if (verificarSeEnderecoEhValido(disco[enderecoInodeIndireto].inodeIndireto.endereco[MAX_INODEINDIRETO-1]))
        {
            listaDiretorioAtualIgualExplorer(disco, disco[enderecoInodeIndireto].inodeIndireto.endereco[MAX_INODEINDIRETO - 1], false);
        }
    }
}

// utilizado para exibir os diretï¿½rios do disco
void listaDiretorioAtualIgualExplorerInodeIndiretoDuplo(Disco disco[], int enderecoInodeIndireto, int ChamadoPeloTriplo = 0)
{

    // verifica se a quantidade de Endereï¿½os no inode indireto nï¿½o estï¿½ cheio
    if (disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista > 0)
    {
        int inicio = 0;
        // adiciona novos blocos enquanto a quantidade for menor que o necessï¿½rio e que nï¿½o esteja cheio
        while (inicio < disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista && inicio < MAX_INODEINDIRETO)
        {
            listaDiretorioAtualIgualExplorerInodeIndiretoSimples(disco, disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio], ChamadoPeloTriplo && inicio + 1 == MAX_INODEINDIRETO);
            inicio++;
        }
    }
}

// utilizado para exibir os diretÃ³rios do disco
void listaDiretorioAtualIgualExplorerInodeIndiretoTriplo(Disco disco[], int enderecoInodeIndireto,  int ChamadoPeloTriplo = 0)
{

    // verifica se a quantidade de Endereï¿½os no inode indireto nï¿½o estï¿½ cheio
    if (disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista > 0)
    {
        int inicio = 0;
        // adiciona novos blocos enquanto a quantidade for menor que o necessï¿½rio e que nï¿½o esteja cheio
        while (inicio < disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista && inicio < MAX_INODEINDIRETO)
        {
            listaDiretorioAtualIgualExplorerInodeIndiretoDuplo(disco, disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio], 1);
            inicio++;
        }
    }
}


void listaDiretorioAtualIgualExplorer(Disco disco[], int localizacaoINodeAtual, bool primeiraVez=true)
{
    int direto, i, enderecoInodeArquivo;
    string blocos;
        
    if (primeiraVez){
        printf("nomenclatura\t\tTipo\t\tBlocos\n");
    }

    for (direto = 0; direto < 5 && verificarSeEnderecoEhValido(disco[localizacaoINodeAtual].inode.enderecoDireto[direto]); direto++)
    {
        for (i = (direto == 0 ? 2 : 0); i < disco[disco[localizacaoINodeAtual].inode.enderecoDireto[direto]].diretorio.tamanhoLista; i++)
        {
            enderecoInodeArquivo = disco[disco[localizacaoINodeAtual].inode.enderecoDireto[direto]].diretorio.arquivo[i].localizacaoINode;

            printf("%s\t\t",disco[disco[localizacaoINodeAtual].inode.enderecoDireto[direto]].diretorio.arquivo[i].nomenclatura);           
            
            if (disco[enderecoInodeArquivo].inode.seguranca[0] == TIPO_ARQUIVO_LINK)
            {
                printf("Link\t\t");
            }
            else if (disco[enderecoInodeArquivo].inode.seguranca[0] == TIPO_ARQUIVO_DIRETORIO)
            {
                printf("Diretorio\t");
            }
            else
            {
                printf("Arquivo\t\t");
            }

            blocos.assign("");
            procurarBlocosDoArquivo(disco, enderecoInodeArquivo, blocos);
            printf("%s\n", blocos.c_str());
        }
    }

    if (!verificarSeEnderecoEhInvalido(disco[localizacaoINodeAtual].inode.enderecoSimplesIndireto))
    {
        listaDiretorioAtualIgualExplorerInodeIndiretoSimples(disco, disco[localizacaoINodeAtual].inode.enderecoSimplesIndireto);
    }

    if (!verificarSeEnderecoEhInvalido(disco[localizacaoINodeAtual].inode.enderecoDuploIndireto))
    {
        listaDiretorioAtualIgualExplorerInodeIndiretoDuplo(disco, disco[localizacaoINodeAtual].inode.enderecoDuploIndireto);
    }

    if (!verificarSeEnderecoEhInvalido(disco[localizacaoINodeAtual].inode.enderecoTriploIndireto))
    {
        listaDiretorioAtualIgualExplorerInodeIndiretoTriplo(disco, disco[localizacaoINodeAtual].inode.enderecoTriploIndireto);
    }
}



void procurarBlocosIntegrosCorrompidos(Disco disco[], int localizacaoINodeAtual, bool primeiraVez=true)
{
    int direto, i, enderecoInodeArquivo;
    string blocos;
        
    if (primeiraVez){
        printf("nomenclatura\t\tStatus\n");
    }

    for (direto = 0; direto < 5 && verificarSeEnderecoEhValido(disco[localizacaoINodeAtual].inode.enderecoDireto[direto]); direto++)
    {
        for (i = (direto == 0 ? 2 : 0); i < disco[disco[localizacaoINodeAtual].inode.enderecoDireto[direto]].diretorio.tamanhoLista; i++)
        {
            enderecoInodeArquivo = disco[disco[localizacaoINodeAtual].inode.enderecoDireto[direto]].diretorio.arquivo[i].localizacaoINode;

            printf("%s\t\t",disco[disco[localizacaoINodeAtual].inode.enderecoDireto[direto]].diretorio.arquivo[i].nomenclatura);

            blocos.assign("");

            procurarBlocosDoArquivo(disco, enderecoInodeArquivo, blocos); 

            vector<string> strBloco;
            strBloco = dividirString(blocos, '-');
            int enderecoBloco;
            int quantidadeBad=0;
            for(const auto& str : strBloco)
            {
                
                enderecoBloco = atoi(str.c_str());
                // printf("\n -- %d -- \n", enderecoBloco);
                if(verificarSeEnderecoEstaDescalibrado(disco, enderecoBloco))
                    quantidadeBad++;
            }

            if (quantidadeBad > 0)
            {
                textcolor(RED);
                printf("[CORROMPIDO]");
                textcolor(WHITE);
            }
            else
            {
                textcolor(GREEN);
                printf("[INTEGRO]");
                textcolor(WHITE);
            }

            printf("\n");
        }
    }

    if (!verificarSeEnderecoEhInvalido(disco[localizacaoINodeAtual].inode.enderecoSimplesIndireto))
    {
        listaDiretorioAtualIgualExplorerInodeIndiretoSimples(disco, disco[localizacaoINodeAtual].inode.enderecoSimplesIndireto);
    }

    if (!verificarSeEnderecoEhInvalido(disco[localizacaoINodeAtual].inode.enderecoDuploIndireto))
    {
        listaDiretorioAtualIgualExplorerInodeIndiretoDuplo(disco, disco[localizacaoINodeAtual].inode.enderecoDuploIndireto);
    }

    if (!verificarSeEnderecoEhInvalido(disco[localizacaoINodeAtual].inode.enderecoTriploIndireto))
    {
        listaDiretorioAtualIgualExplorerInodeIndiretoTriplo(disco, disco[localizacaoINodeAtual].inode.enderecoTriploIndireto);
    }
}

// utilizado para exibir os diretï¿½rios do disco
void listaLinkDiretorioAtualInodeIndiretoSimples(Disco disco[], int enderecoInodeIndireto, int ChamadoPeloTriplo = 0)
{
    int enderecoInodeArquivo;
    int i;

    string blocos;

    int inicio = 0;
    // adiciona novos blocos enquanto a quantidade for menor que o necessï¿½rio e que nï¿½o esteja cheio
    while (inicio < disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista && inicio < MAX_INODEINDIRETO - ChamadoPeloTriplo)
    {

        for (i = 0; i < disco[disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio]].diretorio.tamanhoLista; i++)
        {
            enderecoInodeArquivo = disco[disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio]].diretorio.arquivo[i].localizacaoINode;

            if (disco[enderecoInodeArquivo].inode.seguranca[0] == TIPO_ARQUIVO_LINK || disco[enderecoInodeArquivo].inode.quantidadeLinkFisico > 1)
            {
                printf("%s\t\t",disco[enderecoInodeArquivo].diretorio.arquivo[i].nomenclatura);
                if (disco[enderecoInodeArquivo].inode.seguranca[0] == TIPO_ARQUIVO_LINK)
                {
                    printf("s\t\t");
                }
                else if (disco[enderecoInodeArquivo].inode.quantidadeLinkFisico > 1){
                    printf("Fisico\t\t");
                }

                printf("%d\t\t", enderecoInodeArquivo);
                printf("%d\n", disco[enderecoInodeArquivo].inode.quantidadeLinkFisico);
            }
        }
    
        inicio++;
    }

    if (ChamadoPeloTriplo)
    {
        if (verificarSeEnderecoEhValido(disco[enderecoInodeIndireto].inodeIndireto.endereco[MAX_INODEINDIRETO-1]))
        {
            mostrarLinksNoDiretorio(disco, disco[enderecoInodeIndireto].inodeIndireto.endereco[MAX_INODEINDIRETO - 1], false);
        }
    }
}

// utilizado para exibir os diretï¿½rios do disco
void listaLinkDiretorioAtualInodeIndiretoDuplo(Disco disco[], int enderecoInodeIndireto, int ChamadoPeloTriplo = 0)
{

    // verifica se a quantidade de Endereï¿½os no inode indireto nï¿½o estï¿½ cheio
    if (disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista > 0)
    {
        int inicio = 0;
        // adiciona novos blocos enquanto a quantidade for menor que o necessï¿½rio e que nï¿½o esteja cheio
        while (inicio < disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista && inicio < MAX_INODEINDIRETO)
        {
            listaLinkDiretorioAtualInodeIndiretoSimples(disco, disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio], ChamadoPeloTriplo && inicio + 1 == MAX_INODEINDIRETO);
            inicio++;
        }
    }
}

// utilizado para exibir os diretï¿½rios do disco
void listaLinkDiretorioAtualInodeIndiretoTriplo(Disco disco[], int enderecoInodeIndireto,  int ChamadoPeloTriplo = 0)
{

    // verifica se a quantidade de Endereï¿½os no inode indireto nï¿½o estï¿½ cheio
    if (disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista > 0)
    {
        int inicio = 0;
        // adiciona novos blocos enquanto a quantidade for menor que o necessï¿½rio e que nï¿½o esteja cheio
        while (inicio < disco[enderecoInodeIndireto].inodeIndireto.tamanhoLista && inicio < MAX_INODEINDIRETO)
        {
            listaLinkDiretorioAtualInodeIndiretoDuplo(disco, disco[enderecoInodeIndireto].inodeIndireto.endereco[inicio], 1);
            inicio++;
        }
    }
}


void mostrarLinksNoDiretorio(Disco disco[], int localizacaoINodeAtual, bool primeiraVez=true)
{
    int direto, i, enderecoInodeArquivo;
    string blocos;
        
    if (primeiraVez){
        printf("nomenclatura\t\tTipo\t\tNum. Inode\t\tQtd. Link Fisico\n");
    }

    for (direto = 0; direto < 5 && verificarSeEnderecoEhValido(disco[localizacaoINodeAtual].inode.enderecoDireto[direto]); direto++)
    {
        for (i = (direto == 0 ? 2 : 0); i < disco[disco[localizacaoINodeAtual].inode.enderecoDireto[direto]].diretorio.tamanhoLista; i++)
        {
            enderecoInodeArquivo = disco[disco[localizacaoINodeAtual].inode.enderecoDireto[direto]].diretorio.arquivo[i].localizacaoINode;

            if (disco[enderecoInodeArquivo].inode.seguranca[0] == TIPO_ARQUIVO_LINK || disco[enderecoInodeArquivo].inode.quantidadeLinkFisico > 1)
            {
                printf("%s\t\t",disco[disco[localizacaoINodeAtual].inode.enderecoDireto[direto]].diretorio.arquivo[i].nomenclatura);
                if (disco[enderecoInodeArquivo].inode.seguranca[0] == TIPO_ARQUIVO_LINK)
                {
                    printf("s\t\t");
                }
                else if (disco[enderecoInodeArquivo].inode.quantidadeLinkFisico > 1){
                    printf("Fisico\t\t\t");
                }

                printf("%d\t\t\t", enderecoInodeArquivo);
                printf("%d\n", disco[enderecoInodeArquivo].inode.quantidadeLinkFisico);
            }
        }
    }

    if (!verificarSeEnderecoEhInvalido(disco[localizacaoINodeAtual].inode.enderecoSimplesIndireto))
    {
        listaDiretorioAtualIgualExplorerInodeIndiretoSimples(disco, disco[localizacaoINodeAtual].inode.enderecoSimplesIndireto);
    }

    if (!verificarSeEnderecoEhInvalido(disco[localizacaoINodeAtual].inode.enderecoDuploIndireto))
    {
        listaDiretorioAtualIgualExplorerInodeIndiretoDuplo(disco, disco[localizacaoINodeAtual].inode.enderecoDuploIndireto);
    }

    if (!verificarSeEnderecoEhInvalido(disco[localizacaoINodeAtual].inode.enderecoTriploIndireto))
    {
        listaDiretorioAtualIgualExplorerInodeIndiretoTriplo(disco, disco[localizacaoINodeAtual].inode.enderecoTriploIndireto);
    }
}

int criarDiretorio(Disco disco[], int localizacaoINodeAtual, int localizacaoINodeRaiz, string instrucao)
{
    int i=0;
    bool possivelInserir=true;
    string caminhoAux;
    char nomeDiretorio[MAX_NOME_ARQUIVO];
    int enderecoInodeGravacao = localizacaoINodeAtual;
    vector<string> caminhoCompleto = dividirCaminho(instrucao);

    for(const auto& str : caminhoCompleto)
    {

        if (strcmp(str.c_str(), "/") == 0)
        {
            enderecoInodeGravacao = alterarDiretorio(disco, enderecoInodeGravacao, str.c_str(), localizacaoINodeRaiz, caminhoAux);
        }
        else{
            //verifico se o arquivo atual nï¿½o ï¿½ o ï¿½ltimo do rota. No ï¿½ltimo, vai ser o diretï¿½rio que irei criar
            if (i < caminhoCompleto.size()-1)
            {
                //verifico se o endereï¿½o ï¿½ nulo, se for, nï¿½o hï¿½ como gravar
                if(verificarSeEnderecoEhInvalido(verificarExistenciaArquivoOuDiretorio(disco, enderecoInodeGravacao, str.c_str())))
                {
                    possivelInserir = false;
                    break;
                }
            }
            enderecoInodeGravacao = alterarDiretorio(disco, enderecoInodeGravacao, str.c_str(), localizacaoINodeRaiz, caminhoAux);
        }
        i++;
    }

    if (possivelInserir && verificarSeEnderecoEhValido(enderecoInodeGravacao))
    {
        if (obterUltimaPosicao(caminhoCompleto).size() <= MAX_NOME_ARQUIVO)
        {
            strcpy(nomeDiretorio, obterUltimaPosicao(caminhoCompleto).c_str());
            return inserirDiretorioEArquivo(disco, 'd', enderecoInodeGravacao, nomeDiretorio);
        }
    }
    
    return obterEnderecoInvalido();
}

