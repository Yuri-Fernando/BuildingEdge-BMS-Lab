"use strict";
const $ = id => document.getElementById(id);
const format = (v, digits=0) => Number(v).toLocaleString("pt-BR",{minimumFractionDigits:digits,maximumFractionDigits:digits});
let snapshot=null, history=[], paused=false, offline=false, pending=false, receivedAt=0;
const views={overview:["Visão geral","Visão geral do campus","Telemetria de campo, qualidade dos dados e saúde da ingestão."],devices:["Dispositivos","Dispositivos de campo","Inventário e validade das últimas medições recebidas."],diagnostics:["Diagnósticos","Saúde da ingestão","Contadores reais do coletor e limites de operação."],protocol:["Contrato de dados","Contrato de dados","Do mapa Modbus ao frame binário entre C++ e Rust."]};
function show(view){for(const id of Object.keys(views))$(id).hidden=id!==view;document.querySelectorAll(".nav").forEach(n=>{const active=n.dataset.view===view;n.classList.toggle("active",active);n.setAttribute("aria-pressed",String(active));});[$("breadcrumb").textContent,$("page-title").textContent,$("page-description").textContent]=views[view];}
document.querySelectorAll("[data-view]").forEach(n=>n.addEventListener("click",()=>show(n.dataset.view)));
document.querySelectorAll("[data-jump]").forEach(n=>n.addEventListener("click",()=>show(n.dataset.jump)));
$("search").addEventListener("input",renderTables);$("chart-device").addEventListener("change",renderChart);
$("pause").addEventListener("click",()=>{paused=!paused;$("pause").textContent=paused?"▶ Retomar":"Ⅱ Pausar";if(!paused)refresh();render();});
function isStale(d){return offline||d.age_ms+Math.max(0,performance.now()-receivedAt)>snapshot.stale_after_ms;}
function pill(text,style){return '<span class="pill '+style+'">'+text+'</span>';}
function renderTables(){
 const devices=snapshot?.devices||[];
 function table(items){if(!items.length)return '<div class="empty">'+(devices.length?'Nenhum dispositivo corresponde à busca.':'Aguardando dados do gateway.')+'</div>';return '<table><thead><tr><th>DISPOSITIVO</th><th>VALIDADE</th><th>TEMP.</th><th>CO₂</th><th>UMIDADE</th><th>HVAC</th><th>POTÊNCIA</th><th>ALARME CO₂</th></tr></thead><tbody>'+items.map(d=>{const stale=isStale(d);const val=(v,suffix,digits=0)=>stale?"—":format(v,digits)+suffix;return '<tr><td><div class="device-name"><b>▥</b><div>Sensor BMS · '+d.id+'<small>UNIT 1 · DEVICE '+d.id+'</small></div></div></td><td>'+pill(stale?"Desatualizado":"Atual",stale?"warn":"good")+'</td><td>'+val(d.temp_c," °C",1)+'</td><td>'+val(d.co2_ppm," ppm")+'</td><td>'+val(d.rh_pct," %",1)+'</td><td>'+(stale?pill("Sem validade","neutral"):pill(d.hvac_on?"Ligado":"Desligado",d.hvac_on?"good":"neutral"))+'</td><td>'+val(d.watts," W")+'</td><td>'+(stale?"—":pill(d.co2_alarm?"Ativo":"Normal",d.co2_alarm?"warn":"good"))+'</td></tr>';}).join("")+'</tbody></table>';}
 $("overview-table").innerHTML=table(devices.slice(0,6));const query=$("search").value.trim();$("devices-table").innerHTML=table(devices.filter(d=>String(d.id).includes(query)));
}
function renderChart(){
 const select=$("chart-device"), chosen=select.value, ids=[...new Set(history.map(d=>d.id))];if(ids.join(",")!==[...select.options].map(o=>o.value).join(",")){select.replaceChildren(...ids.map(id=>new Option("BMS · "+id,String(id))));if(ids.some(id=>String(id)===chosen))select.value=chosen;}
 const points=history.filter(d=>String(d.id)===select.value).slice(-120);
 if(points.length<2){$("chart").innerHTML='<div class="empty">Aguardando duas amostras para desenhar o histórico</div>';return;}
 const W=660,H=210,left=48,right=18,top=18,bottom=35,w=W-left-right,h=H-top-bottom;
 const low=Math.floor((Math.min(800,...points.map(p=>p.co2_ppm))-50)/100)*100;
 const high=Math.ceil((Math.max(1100,...points.map(p=>p.co2_ppm))+50)/100)*100;
 const x=i=>left+i*w/(points.length-1), y=v=>top+(high-v)*h/(high-low);
 const line=points.map((p,i)=>(i?"L":"M")+x(i).toFixed(1)+","+y(p.co2_ppm).toFixed(1)).join(" ");
 let grid="";for(let i=0;i<=4;i++){const v=low+(high-low)*i/4;grid+='<line x1="'+left+'" x2="'+(W-right)+'" y1="'+y(v)+'" y2="'+y(v)+'" stroke="#26354b" stroke-dasharray="3 5"/><text x="'+(left-10)+'" y="'+(y(v)+3)+'" text-anchor="end">'+Math.round(v)+'</text>';}
 for(const i of [0,Math.floor(points.length/2),points.length-1]){const t=new Date(points[i].ts_ms).toLocaleTimeString("pt-BR",{hour12:false});grid+='<text x="'+x(i)+'" y="'+(H-10)+'" text-anchor="middle">'+t+'</text>';}
 $("chart").innerHTML='<svg role="img" aria-label="Histórico de CO₂ do dispositivo '+select.value+'" viewBox="0 0 '+W+' '+H+'" preserveAspectRatio="none"><defs><linearGradient id="area" x1="0" y1="0" x2="0" y2="1"><stop stop-color="#62ddb5" stop-opacity=".22"/><stop offset="1" stop-color="#62ddb5" stop-opacity="0"/></linearGradient></defs>'+grid+'<path d="'+line+' L'+x(points.length-1)+','+(H-bottom)+' L'+left+','+(H-bottom)+' Z" fill="url(#area)"/><line x1="'+left+'" x2="'+(W-right)+'" y1="'+y(1000)+'" y2="'+y(1000)+'" stroke="#bd9a5b" stroke-dasharray="5 5"/><path d="'+line+'" fill="none" stroke="#69dfbc" stroke-width="2"/><circle cx="'+x(points.length-1)+'" cy="'+y(points.at(-1).co2_ppm)+'" r="3.5" fill="#8af6d5"/></svg>';
}
function render(){
 const devices=snapshot?.devices||[], current=devices.filter(d=>!isStale(d)), alarms=current.filter(d=>d.co2_alarm).length;
 const status=offline?["API indisponível","bad"]:!snapshot?["Conectando","neutral"]:!devices.length?["Sem dados","neutral"]:!current.length?["Dados desatualizados","warn"]:paused?["Atualização pausada","warn"]:["Coletor conectado","good"];
 $("connection").className="pill "+status[1];if($("connection").textContent!==status[0])$("connection").textContent=status[0];
 const notice=offline?"Não foi possível consultar a API. Medições sem validade até a conexão ser restabelecida.":!devices.length?"Aguardando a primeira amostra do gateway. O processo responde, mas ainda não há telemetria.":!current.length?"Nenhuma medição atual. As últimas amostras permanecem no histórico, sem representar o estado presente.":paused?"Atualização pausada. A validade das medições continua sendo avaliada.":alarms?alarms+" dispositivo(s) com alarme de CO₂ ativo. Regra informativa do laboratório; sem atuação automática.":"Fluxo de telemetria ativo. "+current.length+" dispositivo(s) com dados atuais · atualização a cada 1 segundo.";
 if($("notice").textContent!==notice)$("notice").textContent=notice;$("notice").className="notice "+(offline?"bad":!current.length||paused||alarms?"warn":"good");
 $("devices-count").textContent=snapshot?current.length+" / "+devices.length:"—";
 $("device-sub").textContent=devices.length?current.length+" atuais · "+(devices.length-current.length)+" desatualizados":"Coletor em memória · até 128 dispositivos";
 $("co2").textContent=current.length?format(current.reduce((n,d)=>n+d.co2_ppm,0)/current.length):"—";
 $("temperature").textContent=current.length?format(current.reduce((n,d)=>n+d.temp_c,0)/current.length,1):"—";
 $("power").textContent=current.length?format(current.reduce((n,d)=>n+d.watts,0)):"—";
 for(const k of ["accepted","rejected","evicted"])$(k).textContent=snapshot?format(snapshot[k]):"—";
 $("retained").textContent=snapshot?format(snapshot.history_size)+" / "+format(snapshot.capacity):"—";
 $("health-details").innerHTML="<dt>API HTTP</dt><dd>"+(offline?"Indisponível":"Respondendo")+"</dd><dt>Dispositivos atuais</dt><dd>"+current.length+" / "+devices.length+"</dd><dt>Validade máxima</dt><dd>3 segundos</dd><dt>Limite de conexões</dt><dd>64 ingestão / 32 HTTP</dd><dt>Atualização da tela</dt><dd>"+(paused?"Pausada":"1 segundo")+"</dd><dt>Persistência</dt><dd>Em memória</dd>";
 renderTables();
}
async function refresh(){
 if(paused||pending)return;pending=true;
 try{const r=await fetch("/v1/latest",{cache:"no-store",signal:AbortSignal.timeout(2500)});if(!r.ok)throw Error(r.status);snapshot=await r.json();receivedAt=performance.now();offline=false;$("last-update").textContent="Última consulta · "+new Date().toLocaleTimeString("pt-BR");render();
 const h=await fetch("/v1/history",{cache:"no-store",signal:AbortSignal.timeout(2500)});if(h.ok){history=(await h.json()).samples;renderChart();}}
 catch{offline=true;}finally{pending=false;render();}
}
$("export").addEventListener("click",()=>{if(!history.length){$("notice").textContent="Não há histórico para exportar.";return;}const keys=["id","seq","ts_ms","temp_c","co2_ppm","rh_pct","occupied","hvac_on","co2_alarm","watts"];const csv="\uFEFF"+keys.join(",")+"\r\n"+history.map(d=>keys.map(k=>d[k]).join(",")).join("\r\n");const url=URL.createObjectURL(new Blob([csv],{type:"text/csv;charset=utf-8"}));const a=document.createElement("a");a.href=url;a.download="buildingedge-telemetry.csv";a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);});
refresh();setInterval(refresh,1000);setInterval(render,500);
