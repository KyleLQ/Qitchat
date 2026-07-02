import { useState, useEffect } from 'react';

interface Message {
  sender: string;
  content: string;
}

// Replace with your Droplet's public IP
const API_URL = 'http://localhost:8080/api/messages';

function App() {
  const [messages, setMessages] = useState<Message[]>([]);
  // Generate a random username for the session
  const [sender, setSender] = useState('User' + Math.floor(Math.random() * 1000));
  const [content, setContent] = useState('');

  const fetchMessages = async () => {
    try {
      const res = await fetch(API_URL);
      const data = await res.json();
      setMessages(data || []);
    } catch (e) {
      console.error("Failed to fetch messages", e);
    }
  };

  // Poll the server every 2 seconds for new messages
  useEffect(() => {
    fetchMessages();
    const interval = setInterval(fetchMessages, 2000);
    return () => clearInterval(interval);
  }, []);


  const sendMessage = async (e: React.FormEvent) => {
  // 1. Prevent the browser from reloading the page on form submit
  e.preventDefault(); 
  if (!content.trim()) return;

  // Track the message we are about to send
  const payload = { sender, content: content.trim() };
  console.log("Attempting to send payload:", payload);

  try {
    const res = await fetch(API_URL, {
      method: 'POST',
      headers: { 
        'Content-Type': 'application/json'
      },
      body: JSON.stringify(payload)
    });

    console.log("Server responded with status:", res.status);

    if (res.ok) {
      setContent(''); // Clear the input box only if successful
      fetchMessages(); // Immediately pull latest messages
    } else {
      console.error("Server accepted the request but returned an error status.");
    }
  } catch (error) {
    // This catches the NetworkError you are seeing
    console.error("Fetch intercepted a network error:", error);
  }
};	

  return (
    <div style={{ maxWidth: '600px', margin: '40px auto', fontFamily: 'sans-serif' }}>
      <h2>Minimal Messenger</h2>
      
      <div style={{ 
        border: '1px solid #333', 
        borderRadius: '8px',
        height: '400px', 
        overflowY: 'auto', 
        padding: '20px', 
        marginBottom: '20px',
        backgroundColor: '#fafafa'
      }}>
        {messages.map((m, i) => (
          <div key={i} style={{ marginBottom: '12px' }}>
            <strong style={{ color: '#0066cc' }}>{m.sender}: </strong> 
            <span style={{ color: '#333'}}>{m.content}</span>
          </div>
        ))}
      </div>

      <form onSubmit={sendMessage} style={{ display: 'flex', gap: '10px' }}>
        <input
          value={sender}
          onChange={e => setSender(e.target.value)}
          style={{ width: '100px', padding: '10px', borderRadius: '4px', border: '1px solid #ccc' }}
          title="Username"
        />
        <input
          value={content}
          onChange={e => setContent(e.target.value)}
          style={{ flexGrow: 1, padding: '10px', borderRadius: '4px', border: '1px solid #ccc' }}
          placeholder="Type a message..."
          autoFocus
        />
        <button type="submit" style={{ padding: '10px 20px', cursor: 'pointer' }}>Send</button>
      </form>
    </div>
  );
}

export default App
