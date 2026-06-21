import React, { useEffect, useRef, useState, useCallback } from 'react';
import * as d3 from 'd3';

const API_BASE = 'http://localhost:8080';

export default function App() {
  const svgRef = useRef(null);
  const [topology, setTopology] = useState(null);
  const [status, setStatus] = useState('Loading topology...');
  const [routeQuery, setRouteQuery] = useState({ src: '', dst: '' });
  const [routeResult, setRouteResult] = useState(null);
  const [failQuery, setFailQuery] = useState({ u: '', v: '' });
  const [reconvergeUs, setReconvergeUs] = useState(null);
  const [failedEdges, setFailedEdges] = useState([]);

  const fetchTopology = useCallback(() => {
    fetch(`${API_BASE}/topology`)
      .then((r) => r.json())
      .then((data) => {
        setTopology(data);
        setStatus(`Loaded ${data.nodes.length} routers, ${data.edges.length} links`);
      })
      .catch(() => setStatus('Could not reach backend at localhost:8080 — is it running?'));
  }, []);

  useEffect(() => {
    fetchTopology();
  }, [fetchTopology]);

  useEffect(() => {
    if (!topology || !svgRef.current) return;
    renderGraph(svgRef.current, topology, routeResult?.path, failedEdges);
  }, [topology, routeResult, failedEdges]);

  const handleRouteSubmit = (e) => {
    e.preventDefault();
    fetch(`${API_BASE}/route?src=${routeQuery.src}&dst=${routeQuery.dst}`)
      .then((r) => r.json())
      .then(setRouteResult)
      .catch(() => setRouteResult({ error: 'Request failed' }));
  };

  const handleFailSubmit = (e) => {
    e.preventDefault();
    const u = parseInt(failQuery.u, 10);
    const v = parseInt(failQuery.v, 10);
    fetch(`${API_BASE}/fail`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ u, v }),
    })
      .then((r) => r.json())
      .then((data) => {
        setReconvergeUs(data.reconverge_us);
        setFailedEdges((prev) => [...prev, [u, v]]);
        fetchTopology();
      });
  };

  return (
    <div style={{ display: 'flex', height: '100vh' }}>
      <div style={{ flex: 1, padding: 16 }}>
        <h2 style={{ fontWeight: 500 }}>OSPF routing engine — live topology</h2>
        <p style={{ color: '#999', fontSize: 13 }}>{status}</p>
        <svg ref={svgRef} width="100%" height="600" style={{ background: '#1c1f24', borderRadius: 8 }} />
      </div>

      <div style={{ width: 320, padding: 16, borderLeft: '1px solid #333' }}>
        <h3 style={{ fontWeight: 500 }}>Shortest path</h3>
        <form onSubmit={handleRouteSubmit} style={{ display: 'flex', gap: 8, marginBottom: 8 }}>
          <input
            placeholder="src"
            value={routeQuery.src}
            onChange={(e) => setRouteQuery({ ...routeQuery, src: e.target.value })}
            style={inputStyle}
          />
          <input
            placeholder="dst"
            value={routeQuery.dst}
            onChange={(e) => setRouteQuery({ ...routeQuery, dst: e.target.value })}
            style={inputStyle}
          />
          <button type="submit" style={btnStyle}>Find</button>
        </form>
        {routeResult && !routeResult.error && (
          <p style={{ fontSize: 13 }}>
            Path: {routeResult.path?.join(' → ')} (cost {routeResult.cost})
          </p>
        )}
        {routeResult?.error && <p style={{ fontSize: 13, color: '#e07a7a' }}>No path found</p>}

        <h3 style={{ fontWeight: 500, marginTop: 24 }}>Simulate link failure</h3>
        <form onSubmit={handleFailSubmit} style={{ display: 'flex', gap: 8, marginBottom: 8 }}>
          <input
            placeholder="u"
            value={failQuery.u}
            onChange={(e) => setFailQuery({ ...failQuery, u: e.target.value })}
            style={inputStyle}
          />
          <input
            placeholder="v"
            value={failQuery.v}
            onChange={(e) => setFailQuery({ ...failQuery, v: e.target.value })}
            style={inputStyle}
          />
          <button type="submit" style={btnStyle}>Fail</button>
        </form>
        {reconvergeUs !== null && (
          <p style={{ fontSize: 13 }}>Reconverged in {reconvergeUs} µs</p>
        )}

        <button onClick={fetchTopology} style={{ ...btnStyle, marginTop: 24, width: '100%' }}>
          Refresh topology
        </button>
      </div>
    </div>
  );
}

const inputStyle = {
  width: 50,
  padding: 6,
  background: '#222',
  border: '1px solid #444',
  borderRadius: 4,
  color: '#eee',
};

const btnStyle = {
  padding: '6px 12px',
  background: '#2E75B6',
  border: 'none',
  borderRadius: 4,
  color: '#fff',
  cursor: 'pointer',
};

// D3 force-directed graph renderer. Highlights the active shortest
// path in blue and any failed links in red.
function renderGraph(svgEl, topology, activePath, failedEdges) {
  const svg = d3.select(svgEl);
  svg.selectAll('*').remove();

  const width = svgEl.clientWidth || 700;
  const height = 600;

  const nodes = topology.nodes.map((id) => ({ id }));
  const links = topology.edges.map((e) => ({ source: e.u, target: e.v, cost: e.cost }));

  const pathSet = new Set();
  if (activePath) {
    for (let i = 0; i < activePath.length - 1; i++) {
      pathSet.add(`${activePath[i]}-${activePath[i + 1]}`);
      pathSet.add(`${activePath[i + 1]}-${activePath[i]}`);
    }
  }

  const failedSet = new Set(failedEdges.map(([u, v]) => `${u}-${v}`));
  failedEdges.forEach(([u, v]) => failedSet.add(`${v}-${u}`));

  const simulation = d3
    .forceSimulation(nodes)
    .force('link', d3.forceLink(links).id((d) => d.id).distance(90))
    .force('charge', d3.forceManyBody().strength(-250))
    .force('center', d3.forceCenter(width / 2, height / 2));

  const link = svg
    .append('g')
    .selectAll('line')
    .data(links)
    .join('line')
    .attr('stroke', (d) => {
      const key = `${d.source.id || d.source}-${d.target.id || d.target}`;
      if (pathSet.has(key)) return '#378ADD';
      return '#444';
    })
    .attr('stroke-width', (d) => {
      const key = `${d.source.id || d.source}-${d.target.id || d.target}`;
      return pathSet.has(key) ? 3 : 1.5;
    });

  const node = svg
    .append('g')
    .selectAll('circle')
    .data(nodes)
    .join('circle')
    .attr('r', 16)
    .attr('fill', '#2E75B6')
    .attr('stroke', '#fff')
    .attr('stroke-width', 1)
    .call(drag(simulation));

  const label = svg
    .append('g')
    .selectAll('text')
    .data(nodes)
    .join('text')
    .text((d) => d.id)
    .attr('fill', '#fff')
    .attr('font-size', 12)
    .attr('text-anchor', 'middle')
    .attr('dy', 4);

  simulation.on('tick', () => {
    link
      .attr('x1', (d) => d.source.x)
      .attr('y1', (d) => d.source.y)
      .attr('x2', (d) => d.target.x)
      .attr('y2', (d) => d.target.y);

    node.attr('cx', (d) => d.x).attr('cy', (d) => d.y);
    label.attr('x', (d) => d.x).attr('y', (d) => d.y);
  });

  function drag(sim) {
    return d3
      .drag()
      .on('start', (event, d) => {
        if (!event.active) sim.alphaTarget(0.3).restart();
        d.fx = d.x;
        d.fy = d.y;
      })
      .on('drag', (event, d) => {
        d.fx = event.x;
        d.fy = event.y;
      })
      .on('end', (event, d) => {
        if (!event.active) sim.alphaTarget(0);
        d.fx = null;
        d.fy = null;
      });
  }
}