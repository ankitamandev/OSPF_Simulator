import React, { useEffect, useRef, useState, useCallback } from 'react';
import * as d3 from 'd3';

const API_BASE = 'http://localhost:8080';

// ── Colour tokens ─────────────────────────────────────────────────────────────
const C = {
  bg:        '#0D1117',
  panel:     '#161B22',
  panelAlt:  '#1C2128',
  border:    '#30363D',
  borderLit: '#484F58',
  blue:      '#1B6CA8',
  blueLit:   '#388BCC',
  pathBlue:  '#58A6FF',
  green:     '#3FB950',
  amber:     '#D29922',
  red:       '#F85149',
  textPri:   '#E6EDF3',
  textSec:   '#8B949E',
  textDim:   '#484F58',
  node:      '#1F6FEB',
  nodeCore:  '#388BCC',
  nodeDist:  '#1B6CA8',
  nodeEdge:  '#0D419D',
};

// Router tier classification (mirrors the seed topology comments)
function tier(id) {
  if (id <= 3)  return 'core';
  if (id <= 8)  return 'dist';
  return 'edge';
}

function tierColor(id) {
  const t = tier(id);
  if (t === 'core') return C.nodeCore;
  if (t === 'dist') return C.nodeDist;
  return C.nodeEdge;
}

export default function App() {
  const svgRef     = useRef(null);
  const [topology,    setTopology]    = useState(null);
  const [status,      setStatus]      = useState('Connecting...');
  const [connected,   setConnected]   = useState(false);
  const [routeQuery,  setRouteQuery]  = useState({ src: '', dst: '' });
  const [routeResult, setRouteResult] = useState(null);
  const [failQuery,   setFailQuery]   = useState({ u: '', v: '' });
  const [restoreQuery,setRestoreQuery]= useState({ u: '', v: '', cost: '' });
  const [lastEvent,   setLastEvent]   = useState(null);
  const [failedEdges, setFailedEdges] = useState([]);
  const [loading,     setLoading]     = useState(false);

  const fetchTopology = useCallback(() => {
    fetch(`${API_BASE}/topology`)
      .then(r => r.json())
      .then(data => {
        setTopology(data);
        setConnected(true);
        setStatus(`${data.nodes.length} routers · ${data.edges.length} links`);
      })
      .catch(() => {
        setConnected(false);
        setStatus('Cannot reach backend — is ospf_router running?');
      });
  }, []);

  useEffect(() => { fetchTopology(); }, [fetchTopology]);

  useEffect(() => {
    if (!topology || !svgRef.current) return;
    renderGraph(svgRef.current, topology, routeResult?.path, failedEdges);
  }, [topology, routeResult, failedEdges]);

  const handleRouteSubmit = e => {
    e.preventDefault();
    if (!routeQuery.src || !routeQuery.dst) return;
    setLoading(true);
    fetch(`${API_BASE}/route?src=${routeQuery.src}&dst=${routeQuery.dst}`)
      .then(r => r.json())
      .then(data => { setRouteResult(data); setLoading(false); })
      .catch(() => { setRouteResult({ error: true }); setLoading(false); });
  };

  const handleFailSubmit = e => {
    e.preventDefault();
    const u = parseInt(failQuery.u, 10);
    const v = parseInt(failQuery.v, 10);
    setLoading(true);
    fetch(`${API_BASE}/fail`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ u, v }),
    })
      .then(r => r.json())
      .then(data => {
        setLastEvent({ type: 'fail', u, v, us: data.reconverge_us });
        setFailedEdges(prev => [...prev, [u, v]]);
        setRouteResult(null);
        setLoading(false);
        fetchTopology();
      })
      .catch(() => setLoading(false));
  };

  const handleRestoreSubmit = e => {
    e.preventDefault();
    const u    = parseInt(restoreQuery.u, 10);
    const v    = parseInt(restoreQuery.v, 10);
    const cost = parseInt(restoreQuery.cost, 10) || 10;
    setLoading(true);
    fetch(`${API_BASE}/restore`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ u, v, cost }),
    })
      .then(r => r.json())
      .then(data => {
        setLastEvent({ type: 'restore', u, v, cost, us: data.reconverge_us });
        setFailedEdges(prev => prev.filter(([a, b]) => !(a===u&&b===v) && !(a===v&&b===u)));
        setRouteResult(null);
        setLoading(false);
        fetchTopology();
      })
      .catch(() => setLoading(false));
  };

  const handleRefresh = () => {
    setRouteResult(null);
    setFailedEdges([]);
    setLastEvent(null);
    setRouteQuery({ src: '', dst: '' });
    setFailQuery({ u: '', v: '' });
    setRestoreQuery({ u: '', v: '', cost: '' });
    fetchTopology();
  };

  const nodeCount = topology?.nodes?.length ?? 0;
  const edgeCount = topology?.edges?.length ?? 0;

  return (
    <div style={{ display:'flex', flexDirection:'column', height:'100vh', background:C.bg, color:C.textPri, fontFamily:"'Inter',system-ui,sans-serif", fontSize:13 }}>

      {/* ── Top bar ─────────────────────────────────────────────────────── */}
      <header style={{ display:'flex', alignItems:'center', justifyContent:'space-between', padding:'0 20px', height:52, borderBottom:`1px solid ${C.border}`, background:C.panel, flexShrink:0 }}>
        <div style={{ display:'flex', alignItems:'center', gap:12 }}>
          <span style={{ fontSize:11, fontWeight:600, letterSpacing:'0.12em', textTransform:'uppercase', color:C.blue }}>OSPF</span>
          <span style={{ color:C.border }}>|</span>
          <span style={{ color:C.textSec, fontSize:12 }}>Routing Engine Simulator</span>
        </div>
        <div style={{ display:'flex', alignItems:'center', gap:20 }}>
          <Stat label="Routers" value={nodeCount} />
          <Stat label="Links" value={edgeCount} />
          {lastEvent && (
            <Stat
              label="Last reconverge"
              value={`${lastEvent.us} µs`}
              color={lastEvent.us < 500 ? C.green : C.amber}
            />
          )}
          <div style={{ display:'flex', alignItems:'center', gap:6 }}>
            <div style={{ width:7, height:7, borderRadius:'50%', background: connected ? C.green : C.red }} />
            <span style={{ fontSize:11, color: connected ? C.green : C.red }}>{connected ? 'Connected' : 'Offline'}</span>
          </div>
        </div>
      </header>

      {/* ── Main area ───────────────────────────────────────────────────── */}
      <div style={{ display:'flex', flex:1, overflow:'hidden' }}>

        {/* Graph canvas */}
        <div style={{ flex:1, position:'relative', overflow:'hidden' }}>
          <svg ref={svgRef} width="100%" height="100%" style={{ display:'block' }} />

          {/* Legend */}
          <div style={{ position:'absolute', bottom:16, left:16, background:C.panel, border:`1px solid ${C.border}`, borderRadius:6, padding:'8px 12px', display:'flex', gap:16 }}>
            {[['Core (1–3)', C.nodeCore], ['Distribution (4–8)', C.nodeDist], ['Edge (9–20)', C.nodeEdge]].map(([label, color]) => (
              <div key={label} style={{ display:'flex', alignItems:'center', gap:6 }}>
                <div style={{ width:10, height:10, borderRadius:'50%', background:color }} />
                <span style={{ fontSize:11, color:C.textSec }}>{label}</span>
              </div>
            ))}
            <div style={{ display:'flex', alignItems:'center', gap:6 }}>
              <div style={{ width:20, height:2, background:C.pathBlue, borderRadius:1 }} />
              <span style={{ fontSize:11, color:C.textSec }}>Active path</span>
            </div>
          </div>

          {!topology && (
            <div style={{ position:'absolute', inset:0, display:'flex', alignItems:'center', justifyContent:'center', color:C.textSec, fontSize:13 }}>
              {status}
            </div>
          )}
        </div>

        {/* ── Sidebar ───────────────────────────────────────────────────── */}
        <aside style={{ width:280, borderLeft:`1px solid ${C.border}`, background:C.panel, display:'flex', flexDirection:'column', overflow:'auto', flexShrink:0 }}>

          {/* Shortest path */}
          <Section title="Shortest Path" icon="→">
            <form onSubmit={handleRouteSubmit}>
              <div style={{ display:'grid', gridTemplateColumns:'1fr 1fr', gap:6, marginBottom:8 }}>
                <Field label="Source" value={routeQuery.src} onChange={v => setRouteQuery(q => ({...q, src:v}))} placeholder="e.g. 1" mono />
                <Field label="Dest"   value={routeQuery.dst} onChange={v => setRouteQuery(q => ({...q, dst:v}))} placeholder="e.g. 20" mono />
              </div>
              <Btn type="submit" disabled={loading || !routeQuery.src || !routeQuery.dst}>Find shortest path</Btn>
            </form>
            {routeResult && !routeResult.error && (
              <div style={{ marginTop:10, padding:'10px 12px', background:C.panelAlt, borderRadius:5, border:`1px solid ${C.border}` }}>
                <div style={{ display:'flex', justifyContent:'space-between', marginBottom:6 }}>
                  <span style={{ color:C.textSec, fontSize:11, textTransform:'uppercase', letterSpacing:'0.08em' }}>Path</span>
                  <span style={{ fontFamily:'JetBrains Mono,monospace', fontSize:11, color:C.pathBlue }}>cost {routeResult.cost}</span>
                </div>
                <div style={{ fontFamily:'JetBrains Mono,monospace', fontSize:12, color:C.textPri, lineHeight:1.8, wordBreak:'break-all' }}>
                  {routeResult.path?.join(' → ')}
                </div>
                <div style={{ marginTop:6, fontSize:11, color:C.textSec }}>
                  {routeResult.path?.length - 1} hop{routeResult.path?.length !== 2 ? 's' : ''}
                </div>
              </div>
            )}
            {routeResult?.error && (
              <div style={{ marginTop:8, fontSize:12, color:C.red }}>No path found between those routers.</div>
            )}
          </Section>

          <Divider />

          {/* Fail link */}
          <Section title="Fail Link" icon="✕">
            <form onSubmit={handleFailSubmit}>
              <div style={{ display:'grid', gridTemplateColumns:'1fr 1fr', gap:6, marginBottom:8 }}>
                <Field label="Router U" value={failQuery.u} onChange={v => setFailQuery(q => ({...q, u:v}))} placeholder="e.g. 1" mono />
                <Field label="Router V" value={failQuery.v} onChange={v => setFailQuery(q => ({...q, v:v}))} placeholder="e.g. 2" mono />
              </div>
              <Btn type="submit" disabled={loading || !failQuery.u || !failQuery.v} danger>Simulate failure</Btn>
            </form>
            {lastEvent?.type === 'fail' && (
              <EventBadge color={C.red} label="Link failed" detail={`${lastEvent.u} — ${lastEvent.v}`} us={lastEvent.us} />
            )}
          </Section>

          <Divider />

          {/* Restore link */}
          <Section title="Restore Link" icon="↺">
            <form onSubmit={handleRestoreSubmit}>
              <div style={{ display:'grid', gridTemplateColumns:'1fr 1fr', gap:6, marginBottom:6 }}>
                <Field label="Router U" value={restoreQuery.u} onChange={v => setRestoreQuery(q => ({...q, u:v}))} placeholder="e.g. 1" mono />
                <Field label="Router V" value={restoreQuery.v} onChange={v => setRestoreQuery(q => ({...q, v:v}))} placeholder="e.g. 2" mono />
              </div>
              <Field label="Link cost (default 10)" value={restoreQuery.cost} onChange={v => setRestoreQuery(q => ({...q, cost:v}))} placeholder="e.g. 10" mono />
              <div style={{ marginTop:6 }}>
                <Btn type="submit" disabled={loading || !restoreQuery.u || !restoreQuery.v} success>Restore link</Btn>
              </div>
            </form>
            {lastEvent?.type === 'restore' && (
              <EventBadge color={C.green} label="Link restored" detail={`${lastEvent.u} — ${lastEvent.v} · cost ${lastEvent.cost}`} us={lastEvent.us} />
            )}
          </Section>

          <Divider />

          {/* Controls */}
          <Section title="Network" icon="⊙">
            <Btn onClick={handleRefresh} disabled={loading}>Refresh topology</Btn>
            {status && (
              <div style={{ marginTop:8, fontSize:11, color:C.textSec }}>{status}</div>
            )}
            {failedEdges.length > 0 && (
              <div style={{ marginTop:10 }}>
                <div style={{ fontSize:11, color:C.textSec, textTransform:'uppercase', letterSpacing:'0.08em', marginBottom:4 }}>Failed links</div>
                {failedEdges.map(([u,v], i) => (
                  <div key={i} style={{ fontFamily:'JetBrains Mono,monospace', fontSize:11, color:C.red, lineHeight:1.8 }}>
                    {u} — {v}
                  </div>
                ))}
              </div>
            )}
          </Section>

          {/* API reference */}
          <div style={{ marginTop:'auto', borderTop:`1px solid ${C.border}`, padding:'10px 14px' }}>
            <div style={{ fontSize:10, color:C.textDim, textTransform:'uppercase', letterSpacing:'0.1em', marginBottom:6 }}>API</div>
            {[
              ['GET',  '/route?src=&dst='],
              ['POST', '/fail'],
              ['POST', '/restore'],
              ['GET',  '/table/:id'],
              ['GET',  '/topology'],
            ].map(([method, path]) => (
              <div key={path} style={{ display:'flex', gap:6, fontFamily:'JetBrains Mono,monospace', fontSize:10, color:C.textDim, lineHeight:1.8 }}>
                <span style={{ color: method==='GET' ? C.green : C.amber, minWidth:28 }}>{method}</span>
                <span>{path}</span>
              </div>
            ))}
          </div>

        </aside>
      </div>
    </div>
  );
}

// ── Small reusable components ─────────────────────────────────────────────────

function Stat({ label, value, color }) {
  return (
    <div style={{ textAlign:'center' }}>
      <div style={{ fontFamily:'JetBrains Mono,monospace', fontSize:15, fontWeight:600, color: color || '#E6EDF3', lineHeight:1 }}>{value}</div>
      <div style={{ fontSize:10, color:C.textDim, textTransform:'uppercase', letterSpacing:'0.08em', marginTop:2 }}>{label}</div>
    </div>
  );
}

function Section({ title, icon, children }) {
  return (
    <div style={{ padding:'14px 14px 12px' }}>
      <div style={{ display:'flex', alignItems:'center', gap:6, marginBottom:10 }}>
        <span style={{ fontSize:11, color:C.textDim }}>{icon}</span>
        <span style={{ fontSize:11, fontWeight:600, color:C.textSec, textTransform:'uppercase', letterSpacing:'0.1em' }}>{title}</span>
      </div>
      {children}
    </div>
  );
}

function Divider() {
  return <div style={{ height:1, background:C.border, margin:'0 14px' }} />;
}

function Field({ label, value, onChange, placeholder, mono }) {
  return (
    <div>
      <div style={{ fontSize:10, color:C.textDim, marginBottom:3, textTransform:'uppercase', letterSpacing:'0.08em' }}>{label}</div>
      <input
        value={value}
        onChange={e => onChange(e.target.value)}
        placeholder={placeholder}
        style={{
          width:'100%', padding:'5px 8px', background:C.bg,
          border:`1px solid ${C.border}`, borderRadius:4, color:C.textPri,
          fontFamily: mono ? 'JetBrains Mono,monospace' : 'inherit',
          fontSize:12, outline:'none', boxSizing:'border-box',
        }}
        onFocus={e => e.target.style.borderColor = C.blue}
        onBlur={e  => e.target.style.borderColor = C.border}
      />
    </div>
  );
}

function Btn({ children, danger, success, disabled, type='button', onClick }) {
  const bg = danger ? C.red : success ? C.green : C.blue;
  const bgHov = danger ? '#DA3633' : success ? '#2EA043' : C.blueLit;
  const [hov, setHov] = useState(false);
  return (
    <button
      type={type}
      onClick={onClick}
      disabled={disabled}
      onMouseEnter={() => setHov(true)}
      onMouseLeave={() => setHov(false)}
      style={{
        width:'100%', padding:'6px 10px', border:'none', borderRadius:4, cursor: disabled ? 'not-allowed' : 'pointer',
        background: disabled ? C.border : hov ? bgHov : bg,
        color: disabled ? C.textDim : '#fff',
        fontSize:12, fontWeight:500, transition:'background 0.12s',
      }}
    >
      {children}
    </button>
  );
}

function EventBadge({ color, label, detail, us }) {
  return (
    <div style={{ marginTop:8, padding:'8px 10px', background:C.panelAlt, borderRadius:5, border:`1px solid ${C.border}` }}>
      <div style={{ display:'flex', justifyContent:'space-between', alignItems:'center' }}>
        <span style={{ fontSize:11, color, fontWeight:600 }}>{label}</span>
        <span style={{ fontFamily:'JetBrains Mono,monospace', fontSize:10, color:C.textSec }}>{us} µs</span>
      </div>
      <div style={{ fontFamily:'JetBrains Mono,monospace', fontSize:11, color:C.textSec, marginTop:2 }}>{detail}</div>
    </div>
  );
}

// ── D3 force graph renderer ────────────────────────────────────────────────────
function renderGraph(svgEl, topology, activePath, failedEdges) {
  const svg = d3.select(svgEl);
  svg.selectAll('*').remove();

  const W = svgEl.clientWidth  || 900;
  const H = svgEl.clientHeight || 700;

  const nodes = topology.nodes.map(id => ({ id }));
  const links = topology.edges.map(e => ({ source: e.u, target: e.v, cost: e.cost }));

  const pathSet = new Set();
  if (activePath) {
    for (let i = 0; i < activePath.length - 1; i++) {
      pathSet.add(`${activePath[i]}-${activePath[i+1]}`);
      pathSet.add(`${activePath[i+1]}-${activePath[i]}`);
    }
  }

  const sim = d3.forceSimulation(nodes)
    .force('link',   d3.forceLink(links).id(d => d.id).distance(d => {
      return d.cost < 6 ? 60 : d.cost < 12 ? 90 : 120;
    }))
    .force('charge', d3.forceManyBody().strength(-320))
    .force('center', d3.forceCenter(W / 2, H / 2))
    .force('collision', d3.forceCollide(22));

  const g = svg.append('g');
  svg.call(d3.zoom().scaleExtent([0.3, 3]).on('zoom', e => g.attr('transform', e.transform)));

  const linkEl = g.append('g')
    .selectAll('line')
    .data(links)
    .join('line')
    .attr('stroke', d => {
      const k = `${d.source.id||d.source}-${d.target.id||d.target}`;
      return pathSet.has(k) ? C.pathBlue : C.border;
    })
    .attr('stroke-width', d => {
      const k = `${d.source.id||d.source}-${d.target.id||d.target}`;
      return pathSet.has(k) ? 2.5 : 1;
    })
    .attr('stroke-opacity', d => {
      const k = `${d.source.id||d.source}-${d.target.id||d.target}`;
      return pathSet.has(k) ? 1 : 0.5;
    });

  const costEl = g.append('g')
    .selectAll('text')
    .data(links)
    .join('text')
    .text(d => d.cost)
    .attr('fill', d => {
      const k = `${d.source.id||d.source}-${d.target.id||d.target}`;
      return pathSet.has(k) ? C.pathBlue : C.textDim;
    })
    .attr('font-size', 9)
    .attr('font-family', 'JetBrains Mono,monospace')
    .attr('text-anchor', 'middle')
    .attr('dy', -3);

  const nodeEl = g.append('g')
    .selectAll('circle')
    .data(nodes)
    .join('circle')
    .attr('r', d => tier(d.id) === 'core' ? 18 : tier(d.id) === 'dist' ? 15 : 12)
    .attr('fill', d => tierColor(d.id))
    .attr('stroke', d => {
      if (activePath && activePath.includes(d.id)) return C.pathBlue;
      return C.bg;
    })
    .attr('stroke-width', d => activePath && activePath.includes(d.id) ? 2.5 : 1.5)
    .style('cursor', 'grab')
    .call(d3.drag()
      .on('start', (event, d) => { if (!event.active) sim.alphaTarget(0.3).restart(); d.fx=d.x; d.fy=d.y; })
      .on('drag',  (event, d) => { d.fx=event.x; d.fy=event.y; })
      .on('end',   (event, d) => { if (!event.active) sim.alphaTarget(0); d.fx=null; d.fy=null; })
    );

  const labelEl = g.append('g')
    .selectAll('text')
    .data(nodes)
    .join('text')
    .text(d => d.id)
    .attr('fill', '#fff')
    .attr('font-size', d => tier(d.id) === 'edge' ? 10 : 11)
    .attr('font-family', 'JetBrains Mono,monospace')
    .attr('font-weight', '600')
    .attr('text-anchor', 'middle')
    .attr('dy', 4)
    .style('pointer-events', 'none');

  sim.on('tick', () => {
    linkEl
      .attr('x1', d => d.source.x).attr('y1', d => d.source.y)
      .attr('x2', d => d.target.x).attr('y2', d => d.target.y);
    costEl
      .attr('x', d => (d.source.x + d.target.x) / 2)
      .attr('y', d => (d.source.y + d.target.y) / 2);
    nodeEl.attr('cx', d => d.x).attr('cy', d => d.y);
    labelEl.attr('x', d => d.x).attr('y', d => d.y);
  });
}