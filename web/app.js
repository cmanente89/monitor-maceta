// --- CONFIGURACIÓN DE SUPABASE ---
const SUPABASE_URL = "https://dszgiimsmtboczkndblg.supabase.co";
const SUPABASE_KEY = "sb_publishable_gD9oHAyDE_RDXgb3THEm6w_Z_snNINB";

// Inicializamos la conexión usando la librería oficial
const supabase = window.supabase.createClient(SUPABASE_URL, SUPABASE_KEY);

let graficoChart = null;

const PLANTAS = {
  monstera: { minHumedad: 40, maxHumedad: 80 },
  cactus:   { minHumedad: 15, maxHumedad: 40 },
  helecho:  { minHumedad: 60, maxHumedad: 90 }
};

// --- GESTIÓN DE PESTAÑAS (SOLAPAS) ---
document.querySelectorAll('.btn-pestana').forEach(boton => {
  boton.addEventListener('click', () => {
    document.querySelectorAll('.btn-pestana').forEach(b => b.classList.remove('activa'));
    document.querySelectorAll('.contenido-pestana').forEach(c => c.classList.remove('activa'));

    boton.classList.add('activa');
    const targetId = boton.getAttribute('data-tab');
    document.getElementById(targetId).classList.add('activa');
  });
});

async function obtenerMediciones() {
  const estadoEl = document.getElementById('estado-sistema');
  estadoEl.textContent = "CONSULTANDO_SUPABASE...";

  try {
    // Consulta directa mediante el cliente de Supabase
    const { data: datos, error } = await supabase
      .from('mediciones')
      .select('*')
      .order('created_at', { ascending: false })
      .limit(10);

    if (error) {
      console.error("Error devuelto por Supabase:", error);
      throw error;
    }

    if (!datos || datos.length === 0) {
      estadoEl.textContent = "NO_HAY_DATOS";
      return;
    }

    const ultima = datos[0];
    
    // Solapa 1: Actualizar lecturas actuales
    document.getElementById('humedad-suelo').textContent = ultima.humedad_suelo ?? '--';
    document.getElementById('temp-ambiente').textContent = ultima.temperatura_aire ?? '--';
    document.getElementById('humedad-aire').textContent = ultima.humedad_aire ?? '--';
    
    const fechaOrigen = ultima.created_at || ultima.fecha_hora || new Date();
    const fecha = new Date(fechaOrigen);
    document.getElementById('fecha-lectura').textContent = fecha.toLocaleTimeString();

    evaluarEstadoPlanta(ultima.humedad_suelo);

    // Solapa 2: Dibujar gráfico multivariable
    renderizarGraficoMultivariable(datos.slice().reverse());

  } catch (error) {
    console.error("Error en la petición:", error);
    estadoEl.textContent = "ERROR_DE_CONEXION";
  }
}

function evaluarEstadoPlanta(humedadActual) {
  const tipoPlanta = document.getElementById('select-planta').value;
  const limites = PLANTAS[tipoPlanta];
  const estadoEl = document.getElementById('estado-sistema');

  if (humedadActual < limites.minHumedad) {
    estadoEl.textContent = "ALERTA_RIEGO_REQUERIDO";
    document.body.classList.add('alerta-emergencia');
  } else if (humedadActual > limites.maxHumedad) {
    estadoEl.textContent = "ALERTA_EXCESO_AGUA";
    document.body.classList.add('alerta-emergencia');
  } else {
    estadoEl.textContent = "NOMINAL_OK";
    document.body.classList.remove('alerta-emergencia');
  }
}

function renderizarGraficoMultivariable(datosHistoricos) {
  const ctx = document.getElementById('graficoMultivariable').getContext('2d');

  const etiquetasHoras = datosHistoricos.map(d => {
    const f = new Date(d.created_at || d.fecha_hora || new Date());
    return `${f.getHours()}:${f.getMinutes().toString().padStart(2, '0')}`;
  });

  const datosSuelo = datosHistoricos.map(d => d.humedad_suelo);
  const datosAire = datosHistoricos.map(d => d.humedad_aire);
  const datosTemp = datosHistoricos.map(d => d.temperatura_aire);

  if (graficoChart) {
    graficoChart.destroy();
  }

  graficoChart = new Chart(ctx, {
    type: 'line',
    data: {
      labels: etiquetasHoras,
      datasets: [
        {
          label: 'Hum. Suelo (%)',
          data: datosSuelo,
          borderColor: '#00ff00',
          backgroundColor: 'transparent',
          borderWidth: 2,
          pointBackgroundColor: '#00ff00',
          tension: 0.2
        },
        {
          label: 'Hum. Aire (%)',
          data: datosAire,
          borderColor: '#00ffff',
          backgroundColor: 'transparent',
          borderWidth: 1.5,
          pointBackgroundColor: '#00ffff',
          tension: 0.2
        },
        {
          label: 'Temp (°C)',
          data: datosTemp,
          borderColor: '#ffff00',
          backgroundColor: 'transparent',
          borderWidth: 1.5,
          pointBackgroundColor: '#ffff00',
          tension: 0.2
        }
      ]
    },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      plugins: {
        legend: { display: false }
      },
      scales: {
        x: {
          ticks: { color: '#00ff00', font: { family: 'VT323', size: 14 } },
          grid: { color: 'rgba(0, 255, 0, 0.15)' }
        },
        y: {
          min: 0,
          max: 100,
          ticks: { color: '#00ff00', font: { family: 'VT323', size: 14 } },
          grid: { color: 'rgba(0, 255, 0, 0.15)' }
        }
      }
    }
  });
}

// --- EVENT LISTENERS Y EVENTOS INICIALES ---
document.getElementById('select-planta').addEventListener('change', () => {
  const humedadActual = parseInt(document.getElementById('humedad-suelo').textContent);
  if (!isNaN(humedadActual)) evaluarEstadoPlanta(humedadActual);
});

document.getElementById('btn-recargar').addEventListener('click', obtenerMediciones);

// Carga inicial
obtenerMediciones();

// Polling cada 60s
setInterval(obtenerMediciones, 60000);