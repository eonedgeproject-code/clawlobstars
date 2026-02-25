import React from 'react';
import ReactDOM from 'react-dom/client';
import App from './App';
import SolanaProvider from './lib/SolanaProvider';
import './styles/globals.css';

ReactDOM.createRoot(document.getElementById('root')).render(
  <React.StrictMode>
    <SolanaProvider>
      <App />
    </SolanaProvider>
  </React.StrictMode>
);
