#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <iostream>
#include <string>
#include <time.h>
#include <windows.h>
#include <math.h>
#include <conio.h>

#include "i-node.h"

using namespace std;

void inicializarSistemaDeBlocos(Disco disco[], int quantidadeBlocosTotais){
    int quantidadeBlocosNecessariosListaLivre = quantidadeBlocosTotais / QUANTIDADE_LIMITE_ENDERECO_LISTA_BLOCO_LIVRE;
    
    for(int j=0; j<quantidadeBlocosNecessariosListaLivre; j++)
    {
        inicializarDisco(disco[j]);
        inicializarListaBlocosLivres(disco[j].lbl);
        
    }

    for(int i = quantidadeBlocosNecessariosListaLivre; i < quantidadeBlocosTotais; i++) 
    {
        inicializarDisco(disco[i]);
        adicionarBlocoLivreNaLista(disco, i);
    }
}

void executarSistema(Disco disco[], int quantidadeBlocosTotais, int localizacaoINodeRaiz){
    string rotaAbsoluta;
    int endereco;
    string instrucao;

    instrucao.append("init");
    
    fflush(stdin);

    int localizacaoINodeAtual = localizacaoINodeRaiz;
    
    rotaAbsoluta.append("~");

    printf("\n");
    do
    {		
        if (strcmp(instrucao.substr(0, 2).c_str(), "ls") == 0)
        {
            if (instrucao.size() >= 5 && strcmp(instrucao.substr(3).c_str(), "-li") == 0){
                mostrarLinksNoDiretorio(disco, localizacaoINodeAtual);
            }
            else if (instrucao.size() >= 5 && strcmp(instrucao.substr(3, 2).c_str(), "-detalhado") == 0)
            {
                listarDiretorioDetalhado(disco, localizacaoINodeAtual, strcmp(instrucao.substr(3).c_str(), "-la") == 0);
            }
            else if (instrucao.size() >= 5 && strcmp(instrucao.substr(3).c_str(), "-e") == 0){
                mostrarDiretorioComoExplorer(disco, localizacaoINodeAtual);
            }
            else
            {
                listarConteudoDiretorio(disco, localizacaoINodeAtual, instrucao.size() >= 5 && strcmp(instrucao.substr(3).c_str(), "-a") == 0);
            }

            printf("\n");
        }
        else if (strcmp(instrucao.substr(0, 5).c_str(), "mkdir") == 0)
        {
            if (instrucao.size() >= 6)
            {
                char *comandoEnvio = new char[instrucao.size() + 1];
                strcpy(comandoEnvio, instrucao.substr(6).c_str()); 
                
                if (verificarSeEnderecoEhValido(criarDiretorio(disco, localizacaoINodeAtual, localizacaoINodeRaiz, comandoEnvio)))
                {
                    printf("Diretorio criado\n");
                }
                else
                {
                    textcolor(RED);
                    printf("Nao foi possivel criar o diretorio\n");
                    textcolor(WHITE);
                }
                delete[] comandoEnvio;
            }
        }
        else if (strcmp(instrucao.substr(0, 2).c_str(), "cd") == 0)
        {
            if (instrucao.size() >= 3)
            {
                localizacaoINodeAtual = alterarDiretorio(disco, localizacaoINodeAtual, instrucao.substr(3), localizacaoINodeRaiz, rotaAbsoluta);
            }
        }
        else if (strcmp(instrucao.substr(0, 5).c_str(), "touch") == 0)
        {
            if (instrucao.size() > 6)
            {
                char *touchString = new char[instrucao.size() + 1];
                strcpy(touchString, instrucao.substr(6).c_str());

                if(criarArquivo(disco, localizacaoINodeAtual, localizacaoINodeRaiz, touchString))
                    printf("Arquivo criado\n");
                else {
                    textcolor(RED);
                    printf("Nao foi possivel criar o arquivo\n");
                    textcolor(WHITE);
                }
                delete[] touchString;
            }
        }
        else if(strcmp(instrucao.substr(0, 2).c_str(), "df") == 0)
        {
            int qtdBlocosLivres=0, qtdBlocosOcupados=0;
            float porcentagemBlocosUsados;
            procurarBlocosDisponiveisEOcupados(disco, qtdBlocosLivres, qtdBlocosOcupados, quantidadeBlocosTotais, quantidadeBlocosTotais / QUANTIDADE_LIMITE_ENDERECO_LISTA_BLOCO_LIVRE);

            porcentagemBlocosUsados = (float) qtdBlocosOcupados/quantidadeBlocosTotais;
            printf("Filesystem\tTamanho\tUsados\tDisponivel\tUso%\t\tMontado em\n");
            printf("/dev/sda1\t%d\t%d\t%d\t\t%.2f%%\t\t/\n", quantidadeBlocosTotais, qtdBlocosOcupados,qtdBlocosLivres, porcentagemBlocosUsados*100);
        }
        else if (strcmp(instrucao.substr(0, 2).c_str(), "vi") == 0)
        {
            if (instrucao.size() >= 3)
            {
                string enderecosUtilizados;
                enderecosUtilizados.assign("");

                abrirArquivoParaEdicao(disco, localizacaoINodeAtual, instrucao.substr(3), enderecosUtilizados);
            }
        }
        else if (strcmp(instrucao.substr(0, 5).c_str(), "rmdir") == 0)
        {
            if (instrucao.size() >= 6)
            {
                int contadorDiretorio = 0;
                if(deletarDiretorio(disco, localizacaoINodeAtual, instrucao.substr(6), contadorDiretorio))
                    printf("Diretorio removido\n");
                else {
                    textcolor(RED);
                    printf("Nao foi possivel remover o diretorio\n");
                    textcolor(WHITE);
                }
            }
        }
        else if (strcmp(instrucao.substr(0, 2).c_str(), "rm") == 0)
        {
            if (instrucao.size() >= 3)
            {
                if(deletarArquivo(disco, localizacaoINodeAtual, instrucao.substr(3)))
                    printf("Arquivo removido\n");
                else {
                    textcolor(RED);
                    printf("Nao foi possivel remover o arquivo\n");
                    textcolor(WHITE);
                }
            }
        }  
        else if (strcmp(instrucao.substr(0, 4).c_str(), "link") == 0)
        {
            if (instrucao.size() >= 6)
            {
                if(strcmp(instrucao.substr(5, 2).c_str(), "-s") == 0)
                    criarLinkSimbolico(disco, localizacaoINodeAtual, instrucao.substr(8), localizacaoINodeRaiz);
                else if(strcmp(instrucao.substr(5, 2).c_str(), "-h") == 0)
                    criarLinkFisico(disco, localizacaoINodeAtual, instrucao.substr(8), localizacaoINodeRaiz);
            }
        }   
        else if (strcmp(instrucao.substr(0, 6).c_str(), "unlink") == 0)
        {
            if (instrucao.size() >= 6)
            {
                if(strcmp(instrucao.substr(7, 2).c_str(), "-s") == 0)
                    removerLinkSimbolico(disco, localizacaoINodeAtual, instrucao.substr(10), localizacaoINodeRaiz);
                else if(strcmp(instrucao.substr(7, 2).c_str(), "-h") == 0)
                    removerLinkFisico(disco, localizacaoINodeAtual, instrucao.substr(10), localizacaoINodeRaiz);
            }
        }
        else if (strcmp(instrucao.substr(0, 5).c_str(), "chmod") == 0)
        {
            if (instrucao.size() >= 7)
            {
                definirPermissao(disco, localizacaoINodeAtual, instrucao.substr(6));
            }
        } 
        else if (strcmp(instrucao.c_str(), "disk") == 0)
        {
            printf("\n");
            mostrarDisco(disco, quantidadeBlocosTotais, quantidadeBlocosTotais / QUANTIDADE_LIMITE_ENDERECO_LISTA_BLOCO_LIVRE);
            printf("\n");
        }
        else if (strcmp(instrucao.c_str(), "clear") == 0)
        {
            system("cls");
        }
        else if (strcmp(instrucao.substr(0, 3).c_str(), "bad") == 0)
        {
            if (instrucao.size() >= 4)
            {
                endereco = atoi(instrucao.substr(4).c_str());
                if (endereco >= 0 && endereco < quantidadeBlocosTotais)
                {
                    disco[endereco].bad = 1;
                }else{
                    textcolor(RED);
                    printf("endereco invalido.\n");
                    textcolor(WHITE);
                }
            }
        }
        else if (strcmp(instrucao.substr(0, 8).c_str(), "max file") == 0)
        {
            int quantidadeBlocosDisponiveis = obterQuantidadeBlocosDisponveis(disco);
            int quantidadeUtilizadas = 0;
            int quantidadeRealUtilizada = 0;
            obterQuantidadeBlocosPeloMaiorArquivo(disco, quantidadeBlocosDisponiveis, quantidadeUtilizadas, quantidadeRealUtilizada);

            printf("Pode ser usado %d de %d blocos para inserir um arquivo(%.2f%% aproveitamento)\n", quantidadeRealUtilizada, quantidadeUtilizadas, (float) quantidadeRealUtilizada/ (float)quantidadeUtilizadas*100);
        }
        else if (strcmp(instrucao.substr(0, 10).c_str(), "lost block") == 0)
        {   
            int blocosPerdidos = quantidadeBlocosTotais / QUANTIDADE_LIMITE_ENDERECO_LISTA_BLOCO_LIVRE;
            int blocosPerdidosBytes = obterQuantidadeBlocosPerdidos(quantidadeBlocosTotais);            

            printf("Foram perdidos %d (%d em Bytes) de %d blocos (%.2f%% de perca)\n", blocosPerdidos, blocosPerdidosBytes, quantidadeBlocosTotais, (float) blocosPerdidos/ (float)quantidadeBlocosTotais*100);
        }
        else if (strcmp(instrucao.substr(0, 11).c_str(), "check files") == 0)
        {   
            procurarBlocosIntegrosCorrompidos(disco, localizacaoINodeAtual);
            printf("\n");
        }

        textcolor(GREEN);
        printf("root@localhost");
        textcolor(WHITE);
        printf(":");
        textcolor(LIGHTBLUE);
        printf("%s", rotaAbsoluta.c_str());
        textcolor(WHITE);
        printf("$ ");

		getline(cin, instrucao);
    } while(strcmp(instrucao.c_str(), "exit") != 0);
}

void inicializarSistema(Disco disco[], int quantidadeBlocosTotais)
{
    inicializarSistemaDeBlocos(disco, quantidadeBlocosTotais);
    int localizacaoINodeRaiz = construirDiretorioRaiz(disco);
    executarSistema(disco, quantidadeBlocosTotais, localizacaoINodeRaiz);
}

int obterQuantidadeBlocosTotaisDoUsuario() {

    string charQuantidadeBlocosTotais;
	int quantidadeBlocosTotais;
    bool flag = 1;

    printf("Informe a Quantidade de blocos no disco desejada[ENTER p/ ignorar]:\n");
    while(flag) {

        fflush(stdin);
        getline(cin, charQuantidadeBlocosTotais);

        flag = 0;

        if(strcmp(charQuantidadeBlocosTotais.c_str(),"") == 0) {
			quantidadeBlocosTotais = 1000;
		} 
		else {
			quantidadeBlocosTotais = atoi(charQuantidadeBlocosTotais.c_str()); 
			if(quantidadeBlocosTotais <= 10) {
				printf("A quantidade deve ser maior que 10!\n");
                flag = 1;
			}
		}
    }
	return quantidadeBlocosTotais;
}

int main()
{
    int quantidadeBlocosTotais;
    //no inicio do sistema, deve ser informado pelo usu�rio a quantidade total de discos que vai existir
    
    system("cls");
    
    quantidadeBlocosTotais = obterQuantidadeBlocosTotaisDoUsuario();
    
    printf("Quantidade de blocos selecionada: %d",quantidadeBlocosTotais);
    
    disco disco[quantidadeBlocosTotais];
    inicializarSistema(disco, quantidadeBlocosTotais);
    
    return 0;
}
