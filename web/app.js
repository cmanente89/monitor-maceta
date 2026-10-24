// ==========================================
// INICIALIZACIÓN DE SUPABASE (Usando config.js)
// ==========================================
//esto no deberia estar hardcodeado, igual no hay riesgo
const SUPABASE_URL = "https://dszgiimsmtboczkndblg.supabase.co";
const SUPABASE_KEY = "sb_publishable_gD9oHAyDE_RDXgb3THEm6w_Z_snNINB";


const db = window.supabase.createClient(SUPABASE_URL, SUPABASE_KEY);

let graficoChart = null;
let esModoPruebaActivo = false;

const PLANTAS = {
  monstera: { minHumedad: 40, maxHumedad: 80 },
  cactus:   { minHumedad: 15, maxHumedad: 40 },
  helecho:  { minHumedad: 60, maxHumedad: 90 },
  custom:   { minHumedad: 60, maxHumedad: 80 },
  menta:    { minHumedad: 50, maxHumedad: 85 }
};

// ==========================================
// GESTIÓN DE SOLAPAS (PESTAÑAS)
// ==========================================
document.querySelectorAll('.btn-pestana').forEach(boton => {
  boton.addEventListener('click', () => {
    document.querySelectorAll('.btn-pestana').forEach(b => b.classList.remove('activa'));
    document.querySelectorAll('.contenido-pestana').forEach(c => c.classList.remove('activa'));

    boton.classList.add('activa');
    const targetId = boton.getAttribute('data-tab');
    document.getElementById(targetId).classList.add('activa');
  });
});

// ==========================================
// CONSULTA DE SENSORES Y ESTADO DE PLACA
// ==========================================
async function obtenerMediciones() {
  const estadoEl = document.getElementById('estado-sistema');
  estadoEl.textContent = "CONSULTANDO_SUPABASE...";

  try {
    // 1. Consulta de Sensores (Telemetría)
   const { data: datos, error } = await db
  .from('mediciones')
  .select('id, fecha_hora, humedad_suelo, temperatura_aire, humedad_aire')
  .order('fecha_hora', { ascending: false })
  .limit(10);

    if (error) {
      console.error("Error retornado por Supabase SDK:", error);
      estadoEl.textContent = "ERROR_BD: " + error.message;
    } else if (datos && datos.length > 0) {
      const ultima = datos[0]; 

      document.getElementById('humedad-suelo').textContent = ultima.humedad_suelo ?? '--';
      document.getElementById('temp-ambiente').textContent = ultima.temperatura_aire ?? '--';
      document.getElementById('humedad-aire').textContent = ultima.humedad_aire ?? '--';
      
      if (ultima.fecha_hora) {
  const fecha = new Date(ultima.fecha_hora);
  document.getElementById('fecha-lectura').textContent = fecha.toLocaleString('es-AR', {
    day: '2-digit', month: '2-digit', year: 'numeric',
    hour: '2-digit', minute: '2-digit', second: '2-digit'
  });
}

      evaluarEstadoPlanta(ultima.humedad_suelo);
      renderizarGraficoMultivariable(datos.slice().reverse());
    }

    // 2. Consulta de Estado de Placa (Heartbeat / Last Seen)
    await verificarEstadoPlaca();

  } catch (err) {
    console.error("Error inesperado en app.js:", err);
    estadoEl.textContent = "ERROR_SISTEMA";
  }
}

// ==========================================
// VERIFICACIÓN ONLINE / OFFLINE Y CONTROL
// ==========================================
async function verificarEstadoPlaca() {
  const badgeEl = document.getElementById('badge-online');
  const btnRiego = document.getElementById('btn-iniciar-riego');
  const estadoPruebaEl = document.getElementById('estado-modo-prueba');

  try {
    const { data: ctrl, error } = await db
      .from('irrigation_control')
      .select('*')
      .eq('id', 1)
      .single();

    if (error || !ctrl) return;

    // Verificar si pasaron menos de 15s desde last_seen
    const lastSeenMs = ctrl.last_seen ? new Date(ctrl.last_seen).getTime() : 0;
    const ahoraMs = new Date().getTime();
    const esOnline = (ahoraMs - lastSeenMs) < 15000;

    if (esOnline) {
      badgeEl.textContent = "[ONLINE]";
      badgeEl.style.color = "#00ff00";
    } else {
      badgeEl.textContent = "[OFFLINE]";
      badgeEl.style.color = "#ff0000";
    }

    // Actualizar botón de riego
    if (ctrl.manual_trigger) {
      btnRiego.textContent = "[ REGANDO_AHORA... ]";
      btnRiego.disabled = true;
      btnRiego.style.color = "#ffff00";
      btnRiego.style.borderColor = "#ffff00";
    } else {
      btnRiego.textContent = esOnline ? "[ EJECUTAR_DISPARO_DE_RIEGO ]" : "[ PLACA_OFFLINE ]";
      btnRiego.disabled = !esOnline;
      btnRiego.style.color = esOnline ? "#00ff00" : "#555555";
      btnRiego.style.borderColor = esOnline ? "#00ff00" : "#555555";
    }

    // Actualizar estado de modo prueba
    esModoPruebaActivo = ctrl.test_mode;
    estadoPruebaEl.textContent = ctrl.test_mode ? "ACTIVADO (Ciclo 1 min)" : "DESACTIVADO";
    estadoPruebaEl.style.color = ctrl.test_mode ? "#00ff00" : "#ffff00";

  } catch (err) {
    console.error("Error leyendo irrigation_control:", err);
  }
}

// ==========================================
// DISPARO DE RIEGO MANUAL
// ==========================================
document.getElementById('btn-iniciar-riego').addEventListener('click', async () => {
  const duracion = parseInt(document.getElementById('select-duracion-riego').value);
  const btnRiego = document.getElementById('btn-iniciar-riego');

  btnRiego.textContent = "[ ENVIANDO_ORDEN... ]";
  btnRiego.disabled = true;

  const { error } = await db
    .from('irrigation_control')
    .update({
      manual_trigger: true,
      duration_seconds: duracion
    })
    .eq('id', 1);

  if (error) {
    alert("Error al enviar la orden de riego: " + error.message);
  }
  
  setTimeout(verificarEstadoPlaca, 1000);
});

// ==========================================
// ALTERNAR MODO PRUEBA
// ==========================================
document.getElementById('btn-toggle-prueba').addEventListener('click', async () => {
  const nuevoEstado = !esModoPruebaActivo;

  const { error } = await db
    .from('irrigation_control')
    .update({ test_mode: nuevoEstado })
    .eq('id', 1);

  if (error) {
    alert("Error al cambiar modo prueba: " + error.message);
  } else {
    verificarEstadoPlaca();
  }
});

// ==========================================
// RESETEAR HISTORIAL DE PLANTA (PROCEDIMIENTO SQL)
// ==========================================
document.getElementById('btn-reset-planta').addEventListener('click', async () => {
  if (confirm("¿Seguro que querés borrar el historial de lecturas para iniciar una nueva planta?")) {
    const { error } = await db.rpc('reset_plant_data');
    if (error) {
      alert("Error al reiniciar datos: " + error.message);
    } else {
      alert("Historial reiniciado con éxito.");
      obtenerMediciones();
    }
  }
});

// ==========================================
// LÓGICA DE ALERTA DE PLANTAS
// ==========================================
function evaluarEstadoPlanta(humedadActual) {
  const tipoPlanta = document.getElementById('select-planta').value;
  const limites = PLANTAS[tipoPlanta] || PLANTAS.monstera;
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

// ==========================================
// RENDERING DEL GRÁFICO (CHART.JS)
// ==========================================
function renderizarGraficoMultivariable(datosHistoricos) {
  const ctx = document.getElementById('graficoMultivariable').getContext('2d');

  const etiquetasHoras = datosHistoricos.map(d => {
    if (!d.created_at) return '--:--';
    const f = new Date(d.created_at);
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
      plugins: { legend: { display: false } },
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

// ==========================================
// EVENTOS Y REFRESH AUTOMÁTICO
// ==========================================
document.getElementById('select-planta').addEventListener('change', () => {
  const humedadActual = parseInt(document.getElementById('humedad-suelo').textContent);
  if (!isNaN(humedadActual)) evaluarEstadoPlanta(humedadActual);
});

document.getElementById('btn-recargar').addEventListener('click', obtenerMediciones);

// Carga inicial al refrescar
obtenerMediciones();

// Actualización de estado y datos cada 4 segundos
setInterval(obtenerMediciones, 4000);