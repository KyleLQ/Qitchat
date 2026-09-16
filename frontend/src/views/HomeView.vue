<script setup lang="ts">
import { nextTick, onMounted, onUnmounted, ref, watch } from 'vue'
import { fetchMessages, sendMessage, type ChatMessage } from '../api/messages'

const messages = ref<ChatMessage[]>([])
const sender = ref(localStorage.getItem('qitchat.sender') ?? '')
const draft = ref('')
const loading = ref(true)
const sending = ref(false)
const error = ref<string | null>(null)
const listEl = ref<HTMLElement | null>(null)

let pollTimer: number | undefined

async function load(scroll = false) {
  try {
    messages.value = await fetchMessages()
    error.value = null
    if (scroll) await scrollToBottom()
  } catch (e) {
    error.value = e instanceof Error ? e.message : 'Failed to load messages'
  } finally {
    loading.value = false
  }
}

async function scrollToBottom() {
  await nextTick()
  listEl.value?.scrollTo({ top: listEl.value.scrollHeight })
}

async function handleSend() {
  const name = sender.value.trim()
  const content = draft.value.trim()
  if (!name || !content || sending.value) return
  sending.value = true
  error.value = null
  try {
    await sendMessage(name, content)
    draft.value = ''
    await load(true)
  } catch (e) {
    error.value = e instanceof Error ? e.message : 'Failed to send message'
  } finally {
    sending.value = false
  }
}

watch(sender, (v) => localStorage.setItem('qitchat.sender', v))

// Keep scrolled to bottom when new messages arrive.
watch(
  () => messages.value.length,
  async () => {
    await scrollToBottom()
  },
)

onMounted(async () => {
  await load(true)
  pollTimer = window.setInterval(() => load(), 2000)
})

onUnmounted(() => {
  if (pollTimer !== undefined) window.clearInterval(pollTimer)
})
</script>

<template>
  <main class="chat">
    <section class="chat-card">
      <div class="chat-header">
        <h1>Qitchat</h1>
        <button class="refresh" :disabled="loading" @click="load(true)">Refresh</button>
      </div>

      <label class="name-row">
        <span>Username</span>
        <input v-model="sender" placeholder="Pick a name…" maxlength="64" autocomplete="username" />
      </label>

      <div ref="listEl" class="messages">
        <p v-if="loading" class="status">Loading…</p>
        <p v-else-if="messages.length === 0" class="status">No messages yet. Say hi!</p>
        <div
          v-for="(m, i) in messages"
          :key="i"
          class="message"
          :class="{ own: m.sender === sender.trim() && sender.trim() !== '' }"
        >
          <span class="message-sender">{{ m.sender }}</span>
          <p class="message-content">{{ m.content }}</p>
        </div>
      </div>

      <p v-if="error" class="error">{{ error }}</p>

      <form class="composer" @submit.prevent="handleSend">
        <input
          v-model="draft"
          placeholder="Type a message…"
          maxlength="2000"
          :disabled="sending"
        />
        <button type="submit" :disabled="!sender.trim() || !draft.trim() || sending">
          {{ sending ? 'Sending…' : 'Send' }}
        </button>
      </form>
      <p v-if="!sender.trim()" class="hint">Enter a username to send messages.</p>
    </section>
  </main>
</template>

<style scoped>
.chat {
  display: flex;
  justify-content: center;
  padding: 2rem 1rem;
}

.chat-card {
  width: 100%;
  max-width: 640px;
  display: flex;
  flex-direction: column;
  gap: 0.75rem;
  border: 1px solid var(--color-border);
  border-radius: 12px;
  padding: 1rem;
  background: var(--color-background-soft);
}

.chat-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
}

.chat-header h1 {
  font-size: 1.4rem;
  margin: 0;
}

.refresh {
  border: 1px solid var(--color-border);
  background: var(--color-background);
  border-radius: 8px;
  padding: 0.35rem 0.75rem;
  cursor: pointer;
}

.name-row {
  display: flex;
  align-items: center;
  gap: 0.5rem;
}

.name-row span {
  font-size: 0.85rem;
  white-space: nowrap;
}

.name-row input,
.composer input {
  flex: 1;
  border: 1px solid var(--color-border);
  border-radius: 8px;
  padding: 0.5rem 0.75rem;
  background: var(--color-background);
  color: inherit;
  min-width: 0;
}

.messages {
  display: flex;
  flex-direction: column;
  gap: 0.5rem;
  height: 50vh;
  min-height: 300px;
  overflow-y: auto;
  padding: 0.5rem;
  border: 1px solid var(--color-border);
  border-radius: 8px;
  background: var(--color-background);
}

.status {
  text-align: center;
  opacity: 0.7;
}

.message {
  max-width: 85%;
  align-self: flex-start;
  background: var(--color-background-soft);
  border: 1px solid var(--color-border);
  border-radius: 10px;
  padding: 0.4rem 0.6rem;
  overflow-wrap: anywhere;
}

.message.own {
  align-self: flex-end;
  background: hsla(160, 100%, 37%, 0.15);
}

.message-sender {
  font-size: 0.75rem;
  font-weight: 700;
  opacity: 0.8;
}

.message-content {
  margin: 0.15rem 0 0;
  white-space: pre-wrap;
}

.composer {
  display: flex;
  gap: 0.5rem;
}

.composer button {
  border: none;
  border-radius: 8px;
  padding: 0.5rem 1rem;
  background: hsla(160, 100%, 37%, 1);
  color: white;
  cursor: pointer;
  white-space: nowrap;
}

.composer button:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

.error {
  color: #c0392b;
  margin: 0;
}

.hint {
  margin: 0;
  font-size: 0.8rem;
  opacity: 0.7;
}
</style>
