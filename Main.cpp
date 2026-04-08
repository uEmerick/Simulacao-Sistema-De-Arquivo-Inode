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

// Inicializa o sistema de blocos do disco.
void inicializa_sistema_blocos(Disco disco[], int quantidadeBlocosTotais){
    int quantidadeBlocosNecessariosListaLivre = quantidadeBlocosTotais / QUANTIDADE_LIMITE_ENDERECO_LISTA_BLOCO_LIVRE;
    
    for(int j=0; j<quantidadeBlocosNecessariosListaLivre; j++)
    {
        initDisco(disco[j]);
        initListaBlocosLivre(disco[j].lbl);
        
    }

    for(int i = quantidadeBlocosNecessariosListaLivre; i < quantidadeBlocosTotais; i++) 
    {
        initDisco(disco[i]);
        pushListaBlocoLivre(disco, i);
    }
}

// Executa o sistema de arquivos, interpretando comandos do usuário.
void execucao_sistema(Disco disco[], int quantidadeBlocosTotais, int enderecoInodeRaiz){
    string caminhoAbsoluto;
    int endereco;
    string comando;

    comando.append("init");
    
    fflush(stdin);

    int enderecoInodeAtual = enderecoInodeRaiz;
    
    caminhoAbsoluto.append("~");

    printf("\n");
    do
    {		
                // Se o comando for 'ls' (listar diretório).
        if (strcmp(comando.substr(0, 2).c_str(), "ls") == 0)
        {
            // Se o comando for 'ls -li'.
            if (comando.size() >= 5 && strcmp(comando.substr(3).c_str(), "-li") == 0){
                listaLinkDiretorioAtual(disco, enderecoInodeAtual);
            }
            // Se o comando for 'ls -l' ou 'ls -la'.
            else if (comando.size() >= 5 && strcmp(comando.substr(3, 2).c_str(), "-l") == 0)
            {
                listarDiretorioComAtributos(disco, enderecoInodeAtual, strcmp(comando.substr(3).c_str(), "-la") == 0);
            }
            // Se o comando for 'ls -e'.
            else if (comando.size() >= 5 && strcmp(comando.substr(3).c_str(), "-e") == 0){
                listaDiretorioAtualIgualExplorer(disco, enderecoInodeAtual);
            }
            // Se o comando for 'ls' ou 'ls -a'.
            else
            {
                listarDiretorio(disco, enderecoInodeAtual, comando.size() >= 5 && strcmp(comando.substr(3).c_str(), "-a") == 0);
            }

            printf("\n");
        }
                // Se o comando for 'mkdir' (criar diretório).
        else if (strcmp(comando.substr(0, 5).c_str(), "mkdir") == 0)
        {
            char comandoEnvio[comando.size() + 1];

            if (comando.size() >= 6)
            {
                strcpy(comandoEnvio, comando.substr(6).c_str());

                // Se a criação do diretório for bem-sucedida.
                if (isEnderecoValido(mkdir(disco, enderecoInodeAtual, enderecoInodeRaiz, comandoEnvio)))
                {
                    printf("Diretorio criado\n");
                }
                // Se a criação do diretório falhar.
                else
                {
                    textcolor(RED);
                    printf("Nao foi possivel criar o diretorio\n");
                    textcolor(WHITE);
                }
            }
        }
                // Se o comando for 'cd' (mudar diretório).
        else if (strcmp(comando.substr(0, 2).c_str(), "cd") == 0)
        {
            if (comando.size() >= 3)
            {
                enderecoInodeAtual = cd(disco, enderecoInodeAtual, comando.substr(3), enderecoInodeRaiz, caminhoAbsoluto);
            }
        }
                // Se o comando for 'touch' (criar arquivo).
        else if (strcmp(comando.substr(0, 5).c_str(), "touch") == 0)
        {
            char touchString[comando.size()+1];
            strcpy(touchString, comando.substr(6).c_str());

            if (comando.size() > 6)
                // Se a criação do arquivo for bem-sucedida.
                if(touch(disco, enderecoInodeAtual, enderecoInodeRaiz, touchString))
                    printf("Arquivo criado\n");
                // Se a criação do arquivo falhar.
                else {
                    textcolor(RED);
                    printf("Nao foi possivel criar o arquivo\n");
                    textcolor(WHITE);
                }
                    
        }
                // Se o comando for 'df' (exibir informações do disco).
        else if(strcmp(comando.substr(0, 2).c_str(), "df") == 0)
        {
            int qtdBlocosLivres=0, qtdBlocosOcupados=0;
            float porcentagemBlocosUsados;
            buscaBlocosLivresOcupados(disco, qtdBlocosLivres, qtdBlocosOcupados, quantidadeBlocosTotais, quantidadeBlocosTotais / QUANTIDADE_LIMITE_ENDERECO_LISTA_BLOCO_LIVRE);

            porcentagemBlocosUsados = (float) qtdBlocosOcupados/quantidadeBlocosTotais;
            printf("Filesystem\tTamanho\tUsados\tDisponivel\tUso%\t\tMontado em\n");
            printf("/dev/sda1\t%d\t%d\t%d\t\t%.2f%%\t\t/\n", quantidadeBlocosTotais, qtdBlocosOcupados,qtdBlocosLivres, porcentagemBlocosUsados*100);
        }
                // Se o comando for 'vi' (editar arquivo).
        else if (strcmp(comando.substr(0, 2).c_str(), "vi") == 0)
        {
            if (comando.size() >= 3)
            {
                string enderecosUtilizados;
                enderecosUtilizados.assign("");

                vi(disco, enderecoInodeAtual, comando.substr(3), enderecosUtilizados);
            }
        }
                // Se o comando for 'rmdir' (remover diretório).
        else if (strcmp(comando.substr(0, 5).c_str(), "rmdir") == 0)
        {
            if (comando.size() >= 6)
            {
                int contadorDiretorio = 0;
                // Se a remoção do diretório for bem-sucedida.
                if(rmdir(disco, enderecoInodeAtual, comando.substr(6), contadorDiretorio))
                    printf("Diretorio removido\n");
                // Se a remoção do diretório falhar.
                else {
                    textcolor(RED);
                    printf("Nao foi possivel remover o diretorio\n");
                    textcolor(WHITE);
                }
            }
        }
                // Se o comando for 'rm' (remover arquivo).
        else if (strcmp(comando.substr(0, 2).c_str(), "rm") == 0)
        {
            if (comando.size() >= 3)
            {
                // Se a remoção do arquivo for bem-sucedida.
                if(rm(disco, enderecoInodeAtual, comando.substr(3)))
                    printf("Arquivo removido\n");
                // Se a remoção do arquivo falhar.
                else {
                    textcolor(RED);
                    printf("Nao foi possivel remover o arquivo\n");
                    textcolor(WHITE);
                }
            }
        }  
                // Se o comando for 'link' (criar um link físico ou simbólico).
        else if (strcmp(comando.substr(0, 4).c_str(), "link") == 0)
        {
            if (comando.size() >= 6)
            {
                // Se for um link simbólico.
                if(strcmp(comando.substr(5, 2).c_str(), "-s") == 0)
                    linkSimbolico(disco, enderecoInodeAtual, comando.substr(8), enderecoInodeRaiz);
                // Se for um link físico.
                else if(strcmp(comando.substr(5, 2).c_str(), "-h") == 0)
                    linkFisico(disco, enderecoInodeAtual, comando.substr(8), enderecoInodeRaiz);
            }
        }   
                // Se o comando for 'unlink' (remover um link físico ou simbólico).
        else if (strcmp(comando.substr(0, 6).c_str(), "unlink") == 0)
        {
            if (comando.size() >= 6)
            {
                // Se for um link simbólico.
                if(strcmp(comando.substr(7, 2).c_str(), "-s") == 0)
                    unlinkSimbolico(disco, enderecoInodeAtual, comando.substr(10), enderecoInodeRaiz);
                // Se for um link físico.
                else if(strcmp(comando.substr(7, 2).c_str(), "-h") == 0)
                    unlinkFisico(disco, enderecoInodeAtual, comando.substr(10), enderecoInodeRaiz);
            }
        }
                // Se o comando for 'chmod' (alterar permissões).
        else if (strcmp(comando.substr(0, 5).c_str(), "chmod") == 0)
        {
            if (comando.size() >= 7)
            {
                chmod(disco, enderecoInodeAtual, comando.substr(6));
            }
        } 
                // Se o comando for 'disk' (exibir informações do disco).
        else if (strcmp(comando.c_str(), "disk") == 0)
        {
            printf("\n");
            exibirDisco(disco, quantidadeBlocosTotais, quantidadeBlocosTotais / QUANTIDADE_LIMITE_ENDERECO_LISTA_BLOCO_LIVRE);
            printf("\n");
        }
                // Se o comando for 'clear' (limpar a tela).
        else if (strcmp(comando.c_str(), "clear") == 0)
        {
            system("cls");
        }
                // Se o comando começar com 'bad' (marcar bloco como defeituoso).
        else if (strcmp(comando.substr(0, 3).c_str(), "bad") == 0)
        {
            if (comando.size() >= 4)
            {
                endereco = atoi(comando.substr(4).c_str());
                // Se o endereço for válido.
                if (endereco >= 0 && endereco < quantidadeBlocosTotais)
                {
                    disco[endereco].bad = 1;
                }
                // Se o endereço for inválido.
                else{
                    textcolor(RED);
                    printf("endereco invalido.\n");
                    textcolor(WHITE);
                }
            }
        }
        else if (strcmp(comando.substr(0, 8).c_str(), "max file") == 0)
        {
            int quantidadeBlocosDisponiveis = getQuantidadeBlocosLivres(disco);
            int quantidadeUtilizadas = 0;
            int quantidadeRealUtilizada = 0;
            getQuantidadeBlocosMaiorArquivo(disco, quantidadeBlocosDisponiveis, quantidadeUtilizadas, quantidadeRealUtilizada);

            printf("Pode ser usado %d de %d blocos para inserir um arquivo(%.2f%% aproveitamento)\n", quantidadeRealUtilizada, quantidadeUtilizadas, (float) quantidadeRealUtilizada/ (float)quantidadeUtilizadas*100);
        }
        else if (strcmp(comando.substr(0, 10).c_str(), "lost block") == 0)
        {   
            int blocosPerdidos = quantidadeBlocosTotais / QUANTIDADE_LIMITE_ENDERECO_LISTA_BLOCO_LIVRE;
            int blocosPerdidosBytes = getQuantidadeBlocosPerdidos(quantidadeBlocosTotais);            

            printf("Foram perdidos %d (%d em Bytes) de %d blocos (%.2f%% de perca)\n", blocosPerdidos, blocosPerdidosBytes, quantidadeBlocosTotais, (float) blocosPerdidos/ (float)quantidadeBlocosTotais*100);
        }
        else if (strcmp(comando.substr(0, 11).c_str(), "check files") == 0)
        {   
            buscaBlocosIntegrosCorrompidos(disco, enderecoInodeAtual);
            printf("\n");
        }

        textcolor(GREEN);
        printf("root@localhost");
        textcolor(WHITE);
        printf(":");
        textcolor(LIGHTBLUE);
        printf("%s", caminhoAbsoluto.c_str());
        textcolor(WHITE);
        printf("$ ");

		getline(cin, comando);
    } while(strcmp(comando.c_str(), "exit") != 0);
}

// Inicializa o sistema, criando o diretório raiz e iniciando a execução.
void inicializa_sistema(Disco disco[], int quantidadeBlocosTotais)
{
    inicializa_sistema_blocos(disco, quantidadeBlocosTotais);
    int enderecoInodeRaiz = criaDiretorioRaiz(disco);
    execucao_sistema(disco, quantidadeBlocosTotais, enderecoInodeRaiz);
}

// Obtém a quantidade total de blocos do disco a partir da entrada do usuário.
int obter_quantidade_blocos_totais() {

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

// funcao principal do programa.
int main()
{
    int quantidadeBlocosTotais;
    //no inicio do sistema, deve ser informado pelo usu�rio a quantidade total de discos que vai existir
    
    system("cls");
    
    quantidadeBlocosTotais = obter_quantidade_blocos_totais();
    
    printf("Quantidade de blocos selecionada: %d",quantidadeBlocosTotais);
    
    Disco disco[quantidadeBlocosTotais];
    inicializa_sistema(disco, quantidadeBlocosTotais);
    
    return 0;
}
