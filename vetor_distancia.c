/* Simulador do algoritmo vetor de distância
 * Filipe Nicoli - Teoria de Redes - 2016/1
 
 
 * Algumas notas:
 *
 * - A posição dos roteadores e seus enlaces é fixa (ver diagrama). As
 * conexões de enlaces são feitas à partir de uma matriz, o que, a
 * princípio, torna o programa configurável e escalavel. No entanto,
 * a alteração de valores não foi testada à fundo.
 * 
 * - O usuário deve definir custos para as distâncias. Diferente do
 * protocolo RIP, a métrica é arbitrária e adimensional.
 * 
 * - O programa gera um valor aleatório de espera entre os envios de
 * pacotes para cada roteador. Este valor diz respeito à quantos passos
 * de tempo do programa o roteador irá aguardar até enviar seus pacotes.
 * 
 * 
 * Os roteadores estão conectados da forma abaixo. O programa simulará
 * estas conexões.
 * 
 * 
 *             B ------ D
 *            /| \      |\
 *           / |  \     | \
 *          /  |   \    |  \
 *         A   |    \   |   F
 *          \  |     \  |  /
 *           \ |      \ | /
 *            \|       \|/
 *             C ------ E
 * 
 *  (Diagrama de conexões dos enlaces)
 * 
 *
 */ 


#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* Redes distantes à INF pulos são consideradas inacessíveis.
 * Esta definição ajudará a evitar problemas de contagem infinita. */
#define INFINITO 5


/* Distância à ser utilizada para auto-completar os custos de enlaces
 * quando o modo homônimo for selecionado. */
#define DISTANCIA_AUTOMATICA 1


/* Define quantos passos sem variação nas tabelas são necessários para
 * considerar o algoritmo finalizado. */
#define ESTADO_ESTATICO 10


/* Define o tamanho do buffer de entrada de cada roteador. Um buffer
 * muito pequeno em uma rede grande pode causar perda de pacotes. */
#define PKT_BUFFER 5


#define TEMPO_DE_PASSO 250000

/* Enumeração para assignar IDs aos roteadores.
 * Em uma implementação real, isto não existiria.
 * Como o programa simula o comportamento dos roteadores em rede, é
 * necessário distinguir cada um. Ao invés de endereços, utilizaremos
 * IDs abstraídos pelos nomes roteadorA, roteadorB, ..., roteadorF.
 * 
 * A entrada N_ROTEADORES fica no final e indicará o tamanho da enumeração
 * caso esta seja iniciada em zero e incrementada linearmente. */
enum{roteadorA = 0, roteadorB, roteadorC, roteadorD, roteadorE, roteadorF, N_ROTEADORES};

char * nomes_roteadores[N_ROTEADORES] = {
	
	/* Relativamente auto-explicativo.
	 * Relaciona a enumeração acima à strings imprimíveis. */
	
	"A",
	"B",
	"C",
	"D",
	"E",
	"F"
};


int conexoes_enlaces [N_ROTEADORES][N_ROTEADORES] =
{
	/* Definição das conexões dos enlaces.
	* Reflete o diagrama desenhado acima e printado na tela.
	* É utilizado na etapa de preenchimento das rotas para simplificar
	* e generalizar o código. O valor negativo significa que o roteador
	* não está conectado à mais ninguém. O valor 0 (zero) não pode ser
	* usado pois indica o ID de um roteador válido. */

	{ roteadorB,	roteadorC,			-1,			-1,			-1, 		-1 },	// Conexões do roteador A

	{ roteadorA,	roteadorC,	 roteadorD,  roteadorE,			-1, 		-1 },	// Conexões do roteador B

	{ roteadorA,	roteadorB,	 roteadorE,			-1,			-1, 		-1 },	// Conexões do roteador C

	{ roteadorB,	roteadorE,	 roteadorF,			-1,			-1, 		-1 },	// Conexões do roteador D

	{ roteadorC,	roteadorB,	 roteadorD,	 roteadorF,			-1, 		-1 },	// Conexões do roteador E

	{ roteadorE,	roteadorD,			-1,			-1,			-1, 		-1 },	// Conexões do roteador F

};


typedef struct rota_t{		/* Rota */
	
	/* Estrutura que forma uma rota ideal até um ponto. Indica o destino,
	* o caminho (através de quem) e o custo da rota. */
	
	int destino;
	int caminho;
	int custo;	
}rota_t;



typedef struct pacote_t{		/* Pacote */
	
	/* Pacote simples enviado entre roteadores.
	*  Contém informação sobre o remetente da
	* mensagem e suas rotas ideais até o momento. */
	
	int remetente;
	rota_t rotas [N_ROTEADORES];
} pacote_t;



typedef struct roteador{	/* Roteador */
	
	/* Contém todo o conhecimento de um roteador e algumas variáveis de controle.
	* id: identificação do roteador
	* intervalo: coordena quando o roteador enviará seus pacotes
	* rotas: contém as rotas ideais para cada destino
	* 
	* idx: indexador da pilha de pacotes
	* entrada: buffer de pacotes necessário pela natureza assíncrona da implementação. */
	
	int id;
	int intervalo;
	rota_t rotas [N_ROTEADORES];
	
	/* Por questões de simplicidade o buffer foi implementado como uma
	 * pilha ao invés de uma fila (FIFO). A única diferença é a ordem em
	 * que os pacotes serão processados. */
	
	int idx;
	pacote_t entrada[PKT_BUFFER];
}roteador;


/* Declaração de funções */

// Front-end para a função que preenche os custos.
void preencher_enlaces(roteador *);

// Função que realmente preenche os custos.
void _preencher_enlaces(roteador *, int src, int dst, int custo);

// Simula o envio de pacotes para o buffer de entrada de um roteador.
// Retorna a quantidade de pacotes dropados (por motivos de buffer cheio).
int envia_pacotes(roteador *, int src);

// Simula o recebimento de pacotes e os processa.
// Retorna a quantidade de mudanças na tabela de roteamento.
int recebe_pacote(roteador *, int dst);

// Printa os custos atuais entre roteadores.
void printa_rotas(roteador *);

// Desenha roteadores e seus enlaçes.
// Esta função não acompanharia mudanças na matriz de conexões (o desenho é estático).
void desenha_topologia();


int main(void){
	
	srand(time(NULL));
	roteador roteadores[N_ROTEADORES];
		
	system("clear");
	printf("Simulador de algoritmo vetor de distância\n");
	printf("Filipe Nicoli - Teoria de Redes - 2016/1\n\n");
		
	printf("Topologia de conexão dos roteadores:\n\n");
	
	
	// Desenha o esquema de roteadores na tela
	desenha_topologia();
		
	printf("Preencha os custos de transmissão entre cada roteador:\n");
	preencher_enlaces(roteadores);

	printf("Pressione ENTER para iniciar a simulação.");
	while(getchar()!='\n');
	getchar();
	
	/* Armazenam a contagem de mudanças nas tabelas de roteamento.
	 * São usadas para definir quando o algoritmo chega ao fim. */
	int delta = 0;
	int passo = 0;
	int ultimo_passo_com_variacao = 0;
	
	int pkt_drop = 0;
	
	int r_idx;
	
	// Escolhe um intervalo aleatório inicial entre 0 e 4 para envio de pacote daquele roteador
	for(r_idx = 0; r_idx < N_ROTEADORES; r_idx++)
		roteadores[r_idx].intervalo = (int) (((float)random()/(float)RAND_MAX)*(float)5);
	
	while(1)
	{
		system("clear");
		printf("Simulando... (passo %d) (pkt_drop: %d) (delta anterior: %d)\n\n", passo, pkt_drop, delta);
		printa_rotas(roteadores);
		
		delta = 0;
			
		// Para cada roteador, determina se é hora de enviar novos pacotes
		for(r_idx = 0; r_idx < N_ROTEADORES; r_idx++)
		{
			if(roteadores[r_idx].intervalo)
			{
				roteadores[r_idx].intervalo -= 1;
			}else{
				pkt_drop += envia_pacotes(roteadores, r_idx);
				roteadores[r_idx].intervalo = (int) (((float)random()/(float)RAND_MAX)*5);			
			}
			
		}
		
		
		// Para cada pacote, verifica se novos pacotes chegaram e altera suas opções de rota de acordo
		for(r_idx = 0; r_idx < N_ROTEADORES; r_idx++){
			delta += recebe_pacote(roteadores, r_idx);
		}
		
		
		if(delta) ultimo_passo_com_variacao = passo;
				
		if( passo - ultimo_passo_com_variacao >= ESTADO_ESTATICO ) break;
		
		// Aguarda 1/2 de segundo para que o usuário consiga perceber as variações
		usleep(TEMPO_DE_PASSO);
		
		passo++;
	}
	
	printf("Algoritmo finalizado. Custos ideais encontradas em %d passos.\n", passo-ESTADO_ESTATICO);
	
	printf("Fim.\n");
	return 0;
}

int recebe_pacote(roteador * r, int dst){

	/* Trata os pacotes até que não haja mais nenhum no buffer. */

	int pacote;					// posição do pacote na pilha
	int remetente;				// remetente do pacote
	int custo_atual;			// custo atual até o destino sugerido pela rota de um pacote
	int custo_remetente;		// custo até o remetente do pacote
	int custo_rota_pacote;		// custo da rota sugerida
	int destino_rota_pacote;	// destino da rota sugerida

	int delta = 0;
	int rota_idx;
		
	// Enquanto houverem pacotes a serem recebidos, roda o loop
	while(r[dst].idx!=0)
	{
		
		r[dst].idx--;
		
		for(rota_idx=0; rota_idx<N_ROTEADORES; rota_idx++){

			/* Se o custo da rota que possuímos para o destino especificado pela rota do pacote for
			 * superior ao custo que a rota do pacote apresenta + o custo até o remetente, quer
			 * dizer que o pacote nos apresenta uma rota melhor para um destino. Devemos então
			 * copiar esta rota sugerida pelo pacote e utilizá-la. */
			 
			pacote              = r[dst].idx;											// posição do pacote na pilha
			destino_rota_pacote = r[dst].entrada[ pacote ].rotas[ rota_idx ].destino;	// destino da rota "rota_idx" presente no pacote "pacote"
			custo_atual         = r[dst].rotas[ destino_rota_pacote ].custo;			// custo atual até o destino sugerido pela rota
			remetente           = r[dst].entrada[ pacote ].remetente;					// remetente do pacote
			custo_remetente     = r[dst].rotas[ remetente ].custo;						// custo até o remetente da mensagem
			custo_rota_pacote   = r[dst].entrada[ pacote ].rotas[ rota_idx ].custo;		// custo da rota sugerida
			
			if( custo_atual > (custo_rota_pacote + custo_remetente) )
			
			/* Se o custo atual for maior que o custo até o destino (utilizando o remetente como caminho),
			 * utilizaremos a rota sugerida e utilizaremos o remetente da mensagem como ponte. */
			
			{

				/* Copiamos o remetente como caminho mais curto até o destino. */
				r[dst].rotas[ destino_rota_pacote ].caminho = remetente;

				/* Copiamos o custo e somamos o custo até o vizinho remetente, pois além da distância
				 * de nosso vizinho até o destino, precisamos dar um pulo até o vizinho primeiro. */
				r[dst].rotas[ destino_rota_pacote ].custo   = custo_rota_pacote + custo_remetente;
				
				/* Quando terminarmos de analisar todas as rotas de todos os pacotes,
				 * avisaremos o loop principar de que realizamos mudanças na tabela
				 * do roteador em que estamos atuando. Isto influenciará na decisão
				 * de finalizar o algoritmo. */
				delta++;
			}
		}
	}
	
	return delta;
}

void desenha_topologia()
{
	printf("             B ------ D\n            /| \\      |\\\n           / |  \\     | \\\n          /  |   \\    |  \\\n         A   |    \\   |   F\n          \\  |     \\  |  /\n           \\ |      \\ | /\n            \\|       \\|/\n             C ------ E\n\n");
}

int envia_pacotes(roteador * r, int src){
	
	/* Esta função é dividida em duas partes:
	 * 	- criação do pacote (em formato pacote_t);
	 * 	- envio do pacote.
	 * 
	 *  A criação do pacote segue os moldes de algo que poderia ser real.
	 *  O envio dos pacotes é uma simulação apenas. Como o simulador é
	 * capaz de acessar a memória do buffer de entrada de todos os rotea-
	 * dores, a entrega é feita copiando o pacote no buffer. No mundo real
	 * poderiam ocorrer erros de entrega, mas estes não foram contemplados.
	 *  O máximo que pode ocorrer é o buffer virtual ficar "cheio" (isto é
	 * configurável no início do arquivo), o que pode simular tráfego
	 * intenso no roteador. Quando o buffer fica cheio, o laço de envio
	 * (não o de recebimento) ignora os pacotes e acrescenta uma unidade
	 * ao contador de pacotes dropados. */
	
	
	
	// ------ Cria pacote a ser enviado ------
			pacote_t pkt;
			
			// Define o remetente
			pkt.remetente = src;
			
			// Copia as rotas pessoais para as rotas do pacote
			int dst;
			for( dst=0; dst<N_ROTEADORES; dst++){
				pkt.rotas[dst].destino = r[src].rotas[dst].destino;
				pkt.rotas[dst].caminho = r[src].rotas[dst].caminho;
				pkt.rotas[dst].custo   = r[src].rotas[dst].custo;
			}
	// ---------- Pacote finalizado -----------
	
	
	
	
	// ----------- Inicia envio -----------
	
	/* O envio aqui é representado pela cópia do pacote no buffer de
	 * entrada do roteador.
	 * - "rota_dst" é o indexador da lista de rotas contidas no pacote
	 * - "existe" é utilizada para informar se um destino na lista de
	 * roteadores deve receber ou não o pacote. A verificação se baseia
	 * na existência de um link físico entre os dispositivos (configurado
	 * na matriz conexoes_enlaces). Esta variável é necessária para que
	 * o simulador proteja dispositivos desconectados do remetente de
	 * receberem uma mensagem impossível de ser recebida no mundo real.
	 * - "dst_idx" é o indexador da lista de roteadores.
	 * - "pkt_drop" é a contagem de pacotes que não puderam ser entregues
	 * aos roteadores. Este valor é retornado pela função e é somado à
	 * variável de mesma função no laço principal. */
	 
	int rota_dst, existe, dst_idx, pkt_drop = 0;;
	
	
	// Envia o pacote para cada roteador ao alcance.
	for (dst=0; dst<N_ROTEADORES; dst++){
		
		/* Aqui testamos se o roteados designado por dst existe na lista
		 * de enlaces do roteador remetente. */
		existe = 0;
		for(dst_idx=0; dst_idx<N_ROTEADORES; dst_idx++)
			if(conexoes_enlaces[src][dst_idx] == dst)
				existe = 1;
		
		/* Se o roteador apontado por dst não existir na lista de
		 * enlaces, significa que não é vizinho do remetente (src) e não
		 * deve receber a mensagem. Este tipo de verificação, obviamente,
		 * só é necessária pois estamos simulando a rede. Na prática,
		 * os roteadores não-vizinhos não receberiam o pacote
		 * simplesmente por não estarem conectados. */
		if(existe){

			// Testa se o buffer do destinatário está cheio.
			if(r[dst].idx == (PKT_BUFFER))
			{
				pkt_drop++;
			}
			else
			{

				// Insere o remetende no buffer do destinatário.
				r[dst].entrada[r[dst].idx].remetente = pkt.remetente;
				
				// Copia todas as rotas do pacote para o buffer do destinatário.
				for(rota_dst = 0; rota_dst<N_ROTEADORES; rota_dst++){
					
					r[dst].entrada[r[dst].idx].rotas[rota_dst].destino = pkt.rotas[rota_dst].destino;
					r[dst].entrada[r[dst].idx].rotas[rota_dst].caminho = pkt.rotas[rota_dst].caminho;
					r[dst].entrada[r[dst].idx].rotas[rota_dst].custo   = pkt.rotas[rota_dst].custo;

				}
				r[dst].idx++;
				
			}
		}
	}
	// --------- Envio finalizado ---------
	
	return pkt_drop;
}

void printa_rotas(roteador * r){
	
	/* Percorre todos os roteadores printando as distâncias entre todos
	 * eles quando a distância não for em relação à si mesmo. */
		
	int i, j;
	for(i=0; i<N_ROTEADORES; i++)
	{
		for(j=0; j<N_ROTEADORES; j++)
		{

			if(i!=j)
				if(r[i].rotas[j].custo == INFINITO)
					printf("C(%s,%s)=INF\n", nomes_roteadores[i], nomes_roteadores[j], r[i].rotas[j].custo);
				else
					printf("C(%s,%s)=%d por %s\n", nomes_roteadores[i], nomes_roteadores[j], r[i].rotas[j].custo, nomes_roteadores[r[i].rotas[j].caminho]);

		}
		printf("\n");
	}
}

void preencher_enlaces(roteador * roteadores){
	
	/* Função responsável por caminhar pela matriz conexoes_enlaces[][]
	 * e requisitar os custos de enlace. Se o usuário se cansar de inserir
	 * valores, pode digitar 0 (zero) e o programa completará o resto dos
	 * custos com valor definido pela macro DISTANCIA_AUTOMATICA. */
	 
	int custo;
	int i, j;
	int autopreencher = 0;
	int conta = 0;
	
	printf("\n\tDICA: Para auto-preencher o resto da tabela com custo 1,\n\t      insira custo zero a qualquer momento.\n\n");
	
	// Define custo infinito para tudo e zera idx
	for(i=0; i<N_ROTEADORES; i++){
		
		roteadores[i].idx = 0;
		
		for(j=0; j<N_ROTEADORES; j++){
			_preencher_enlaces(roteadores, i, j, INFINITO);
		}
	}
	
	for(i=0; i<N_ROTEADORES; i++){
		for(j=0; j<N_ROTEADORES; j++){
			
			// Para cara enlace existente entre dois dispositivos...
			if(conexoes_enlaces[i][j]!=-1){
				conta++;
				printf("C(%s,%s)=", nomes_roteadores[i], nomes_roteadores[conexoes_enlaces[i][j]]);
				if(!autopreencher){
					scanf("%d", &custo);
					if(custo == 0){
						printf("Preenchendo o resto dos custos de enlace com %d.\n", DISTANCIA_AUTOMATICA);
						printf("C(%s,%s)=%d\n", nomes_roteadores[i], nomes_roteadores[conexoes_enlaces[i][j]], DISTANCIA_AUTOMATICA);
						custo = DISTANCIA_AUTOMATICA;
						autopreencher = 1;
					}
				}else
					printf("%d\n", custo);

				_preencher_enlaces(roteadores, i, conexoes_enlaces[i][j], custo);
			}
		}
	}
	printf("\n%d custos definidos.\n%d enlaces presentes.\n\n", conta, conta/2);
}

void _preencher_enlaces(roteador * r, int src, int dst, int custo){
	
	/* Preenche a rota destinada àquele roteador (rotas[dst]) com o
	 * destino, caminho (o próprio destino neste caso) e o custo. */
	 
	r[src].rotas[dst].destino = dst;
	
	if(custo==INFINITO)
		r[src].rotas[dst].caminho = -1;
	else
		r[src].rotas[dst].caminho = dst;

	r[src].rotas[dst].custo = custo;
}
