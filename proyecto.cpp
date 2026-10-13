/* 
 * Proyecto 5: Implementacion de un Cache Multinivel (L1, L2, L3)
 */

#include <iostream>
#include <vector>
#include <cstdint>
#include <iomanip>
#include <random> 

using namespace std; 

// Estados simplificados para la coherencia de datos
enum EstadoMESI{
    MODIFICADO,
    EXCLUSIVO,
    COMPARTIDO,
    INVALIDO
};

// Estructura de control para gestionar los datos
struct InfoAcceso{
    bool acierto;
    bool hubo_desalojo;
    uint32_t dir_desalojada;
    bool estaba_modificado;
};

// Representacion de un bloque de cache
struct BloqueCache{
    bool valido = false;
    uint32_t etiqueta = 0;
    EstadoMESI estado = INVALIDO;
    int lru_contador = 0; 
};

template <size_t Vias>
struct Conjunto{
    vector<BloqueCache> bloques;

    Conjunto(){
        bloques.resize(Vias);
    }
};

// Plantilla para cualquier nivel de cache
template <size_t CapacidadKB, size_t Vias, size_t Latencia, size_t TamBloque = 64>
class NivelCache{
    private:
        int bytes_cap, num_conjuntos;
        vector<Conjunto<Vias>> conjuntos;
        
        int aciertos = 0, fallos = 0, reloj_global = 0;

    public:
        NivelCache(){
            bytes_cap = CapacidadKB * 1024;
            num_conjuntos = bytes_cap / (Vias * TamBloque);
            conjuntos.resize(num_conjuntos);
        }

        // Descomposicion de la direccion de memoria
        void decodificarDir(uint32_t dir, uint32_t& etiqueta, uint32_t& indice, uint32_t& offset){
            offset = dir % TamBloque;
            
            indice = (dir / TamBloque) % num_conjuntos;
            
            etiqueta = dir / (TamBloque * num_conjuntos);
        }

        uint32_t reconstruirDir(uint32_t etiqueta, uint32_t indice){
            return (etiqueta * num_conjuntos * TamBloque) + (indice * TamBloque);
        }
        
        // Retorna un InfoAcceso que le indica al sistema si debe enviar datos a un nivel inferior
        InfoAcceso acceder(uint32_t dir, bool es_escritura){
            reloj_global++;
            uint32_t etiqueta, indice, offset;
            decodificarDir(dir, etiqueta, indice, offset);

            Conjunto<Vias>& conjunto_actual = conjuntos[indice];

            for(size_t i = 0; i < Vias; i++){
                if(conjunto_actual.bloques[i].valido && conjunto_actual.bloques[i].etiqueta == etiqueta){
                    aciertos++;
                    conjunto_actual.bloques[i].lru_contador = reloj_global;
                    
                    if(es_escritura){
                        conjunto_actual.bloques[i].estado = MODIFICADO;
                    }
                    return {true, false, 0, false}; 
                }
            }

            fallos++;
            int indice_lru = 0;
            int min_lru = 999999999; // Numero grande para asegurar que se sobreescriba en el for
            
            // Busca el bloque mas antiguo o uno vacio
            for(size_t i = 0; i < Vias; i++){
                if(!conjunto_actual.bloques[i].valido){
                    indice_lru = i;
                    break; // Hay un bloque vacio
                }

                if(conjunto_actual.bloques[i].lru_contador < min_lru){
                    min_lru = conjunto_actual.bloques[i].lru_contador;
                    indice_lru = i;
                }
            }

            bool hay_desalojo = conjunto_actual.bloques[indice_lru].valido;
            uint32_t dir_victima = 0;
            if(hay_desalojo){
                dir_victima = reconstruirDir(conjunto_actual.bloques[indice_lru].etiqueta, indice);
            }
            bool victima_mod = hay_desalojo && (conjunto_actual.bloques[indice_lru].estado == MODIFICADO);

            // Guardar nuevo bloque
            conjunto_actual.bloques[indice_lru].valido = true;
            conjunto_actual.bloques[indice_lru].etiqueta = etiqueta;
            conjunto_actual.bloques[indice_lru].lru_contador = reloj_global;
            
            if(es_escritura){
                conjunto_actual.bloques[indice_lru].estado = MODIFICADO;
            }else{
                conjunto_actual.bloques[indice_lru].estado = EXCLUSIVO;
            }

            return {false, hay_desalojo, dir_victima, victima_mod};
        }

        double tasaFallos(){
            if (aciertos + fallos == 0) return 0.0;
            return (double)fallos / (aciertos + fallos); 
        }
        
        int getLatencia() { return Latencia; }
};

class SistemaCache{
    private:
        NivelCache<32, 4, 4> L1;     // 32 KB, 4 vias, 4 ciclos
        NivelCache<256, 8, 12> L2;   // 256 KB, 8 vias, 12 ciclos
        NivelCache<8192, 16, 40> L3; // 8 MB, 16 vias, 40 ciclos
        
        const int latencia_ram = 80;
        int total_peticiones = 0;

        void propagarWriteBack(uint32_t dir, int nivel_origen){
            if(nivel_origen == 1){
                // Va de L1 a L2
                InfoAcceso resL2 = L2.acceder(dir, true);
                if(resL2.hubo_desalojo && resL2.estaba_modificado){
                    propagarWriteBack(resL2.dir_desalojada, 2);
                }
            }else if(nivel_origen == 2){
                // Va de L2 a L3
                L3.acceder(dir, true);
            }
        }

    public:
        // Simulacion de peticion de lectura o escritura
        void peticionCPU(uint32_t dir, bool es_escritura){
            total_peticiones++;
                        
            // Buscar en L1
            InfoAcceso resL1 = L1.acceder(dir, es_escritura);
            if(resL1.hubo_desalojo && resL1.estaba_modificado){
                propagarWriteBack(resL1.dir_desalojada, 1); 
            }
            if(resL1.acierto) return;

            // No estaba en L1, buscar en L2
            InfoAcceso resL2 = L2.acceder(dir, false); 
            if(resL2.hubo_desalojo && resL2.estaba_modificado){
                propagarWriteBack(resL2.dir_desalojada, 2); 
            }
            if(resL2.acierto) return;

            // Miss en L2, va directo a L3
            L3.acceder(dir, false);
        }

        void imprimirEstadisticas(){
            double mr1 = L1.tasaFallos();
            double mr2 = L2.tasaFallos();
            double mr3 = L3.tasaFallos();

            double amat = L1.getLatencia() + mr1 * (L2.getLatencia() + mr2 * (L3.getLatencia() + mr3 * latencia_ram));

            cout << "\n=== Resultados de la Simulacion ===\n";
            cout << "Peticiones totales procesadas: " << total_peticiones << "\n";
            cout << fixed << setprecision(3);
            cout << "Miss rate L1: " << mr1 * 100 << "%\n";
            cout << "Miss rate L2: " << mr2 * 100 << "%\n";
            cout << "Miss rate L3: " << mr3 * 100 << "%\n";
            cout << "AMAT Calculado: " << amat << " ciclos\n";
            cout << "===================================\n\n";
        }
};

int main(){
    SistemaCache simulador;
    cout << "Iniciando simulador multinivel...\n";

    // 1. Carga sintetica
    cout << "Metiendo datos sinteticos...\n";
    for(int i = 0; i < 10000; i++){
        uint32_t dir = (i % 100) * 16; 
        simulador.peticionCPU(dir, false); 
    }

    // 2. Carga aleatoria (evalua el comportamiento del cache bajo comportamiento impredecible)
    cout << "Metiendo datos aleatorios...\n";
    mt19937 generador(12345); // Semilla fija para que siempre de el mismo output en las pruebas
    uniform_int_distribution<uint32_t> rango(0, 1024 * 1024); 
    
    for(int i = 0; i < 5000; i++){
        uint32_t dir = rango(generador);
        bool escribir = (i % 3 == 0); 
        simulador.peticionCPU(dir, escribir);  
    }

    simulador.imprimirEstadisticas();
    return 0;
}
