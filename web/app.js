// --- CONFIGURACIÓN DE CREDENCIALES ---
// Reemplazar con tus datos reales localmente. Para producción usaremos variables de entorno.
const supabaseUrl = "https://dszgiimsmtboczkndblg.supabase.co/rest/v1/";
const supabaseKey = "sb_publishable_EiRKDJn1DV1E9tlBnSyXsA_MvAzPnx3";

const perfilesPlantas = {
    cactus: { minSuelo: 15, maxTemp: 35 },
    monstera: { minSuelo: 40, maxTemp: 30 },
    helecho: { minSuelo: 60, maxTemp: 26 }
};

let perfilActual = perfilesPlantas.monstera;

// Escuchador para el cambio de planta seleccionada
document.getElementById("selPlanta").addEventListener("change", (e) => {
    perfilActual = perfilesPlantas[e.target.value];
    evaluarAlertas();
});

// Petición a la base de datos de Supabase
async function obtenerDatos() {
    try {
        const respuesta = await fetch(`${SUPABASE_URL}?order=id.desc&limit=1`, {
            method: 'GET',
            headers: { 
                'apikey': SUPABASE_KEY, 
                'Authorization': `Bearer ${SUPABASE_KEY}` 
            }
        });
        const datos = await respuesta.json();
        
        if (datos.length > 0) {
            const ultimaMedicion = datos[0];
            
            // Renderizado en la interfaz gráfica
            document.getElementById("lblSuelo").innerText = `${ultimaMedicion.humedad_suelo}%`;
            document.getElementById("lblTemp").innerText = ultimaMedicion.temperatura_aire;
            document.getElementById("lblHumedad").innerText = ultimaMedicion.humedad_aire;
            
            // Guardado temporal de estados para la lógica interna
            document.getElementById("lblSuelo").dataset.valor = ultimaMedicion.humedad_suelo;
            document.getElementById("lblTemp").dataset.valor = ultimaMedicion.temperatura_aire;

            const fecha = new Date(ultimaMedicion.fecha_hora);
            document.getElementById("lblActualizado").innerText = `ULTIMA_SINC: ${fecha.toLocaleTimeString()}`;
            
            evaluarAlertas();
        }
    } catch (e) { 
        document.getElementById("lblAlerta").innerText = "ERR_RED_CONEXION_FALLIDA";
        document.getElementById("lblAlerta").className = "alerta-critica";
    }
}

// Lógica de control de alertas analógicas
function evaluarAlertas() {
    const hSuelo = parseInt(document.getElementById("lblSuelo").dataset.valor) || 0;
    const temp = parseFloat(document.getElementById("lblTemp").dataset.valor) || 0;
    
    let msgAlerta = "NOMINAL_OK";
    let critico = false;

    if (hSuelo < perfilActual.minSuelo) {
        msgAlerta = "ERR_CRITICO_BAJO_NIVEL_H2O";
        critico = true;
    } else if (temp > perfilActual.maxTemp) {
        msgAlerta = "ERR_CRITICO_SOBRECALENTAMIENTO";
        critico = true;
    }

    const lblAlerta = document.getElementById("lblAlerta");
    lblAlerta.innerText = msgAlerta;
    
    if (critico) {
        lblAlerta.className = "alerta-critica";
    } else {
        lblAlerta.className = "";
    }
}

// Botón manual de refresco
document.getElementById("btnActualizar").addEventListener("click", obtenerDatos);

// Inicialización automática de la terminal
obtenerDatos();
setInterval(obtenerDatos, 10000); // Consulta cada 10 segundos