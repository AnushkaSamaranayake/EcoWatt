import React, { useState, useEffect } from 'react'

const ErrorInjectionPanel = ({ apiBase, selectedDevice }) => {
  const [errorType, setErrorType] = useState('EXCEPTION')
  const [exceptionCode, setExceptionCode] = useState(4)
  const [delayMs, setDelayMs] = useState(5000)
  const [injectionHistory, setInjectionHistory] = useState([])
  const [loading, setLoading] = useState(false)
  const [message, setMessage] = useState('')

  const errorTypes = [
    { value: 'EXCEPTION', label: 'Modbus Exception', description: 'InverterSim sends Modbus exception frame' },
    { value: 'CRC_ERROR', label: 'CRC Error', description: 'InverterSim sends frame with bad CRC' },
    { value: 'CORRUPT', label: 'Corrupt Frame', description: 'InverterSim sends corrupted data frame' },
    { value: 'PACKET_DROP', label: 'Packet Drop', description: 'InverterSim drops packets (no response)' },
    { value: 'DELAY', label: 'Delay/Timeout', description: 'InverterSim delays response' }
  ]

  const exceptionCodes = [
    { value: 1, label: 'Illegal Function' },
    { value: 2, label: 'Illegal Data Address' },
    { value: 3, label: 'Illegal Data Value' },
    { value: 4, label: 'Slave Device Failure' },
    { value: 5, label: 'Acknowledge' },
    { value: 6, label: 'Slave Device Busy' },
    { value: 8, label: 'Memory Parity Error' },
    { value: 10, label: 'Gateway Path Unavailable' },
    { value: 11, label: 'Gateway Target Device Failed' }
  ]

  // Fetch error injection history
  const fetchInjectionHistory = async () => {
    try {
      const response = await fetch(`${apiBase}/api/error-flag/status`)
      if (response.ok) {
        const data = await response.json()
        setInjectionHistory(data.history || [])
      }
    } catch (error) {
      console.error('Error fetching injection history:', error)
    }
  }

  useEffect(() => {
    fetchInjectionHistory()
    const interval = setInterval(fetchInjectionHistory, 5000) // Poll every 5 seconds
    return () => clearInterval(interval)
  }, [apiBase])

  const injectError = async () => {
    setLoading(true)
    setMessage('')

    try {
      const payload = {
        device_id: selectedDevice,
        errorType: errorType,
        exceptionCode: errorType === 'EXCEPTION' ? parseInt(exceptionCode) : 0,
        delayMs: errorType === 'DELAY' ? parseInt(delayMs) : 0
      }

      const response = await fetch(`${apiBase}/api/error-flag/add`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json'
        },
        body: JSON.stringify(payload)
      })

      const data = await response.json()

      if (response.ok) {
        setMessage(`${data.message}`)
        fetchInjectionHistory()
      } else {
        setMessage(`Error: ${data.error || data.message}`)
      }
    } catch (error) {
      setMessage(`Failed to set error flag: ${error.message}`)
    } finally {
      setLoading(false)
    }
  }

  const clearHistoryForDevice = async (deviceId) => {
    try {
      const response = await fetch(`${apiBase}/api/error-flag/clear/${deviceId}`, {
        method: 'POST'
      })
      if (response.ok) {
        setMessage(`Cleared history for ${deviceId}`)
        fetchInjectionHistory()
      }
    } catch (error) {
      setMessage(`Failed to clear history: ${error.message}`)
    }
  }

  const clearAllHistory = async () => {
    try {
      const response = await fetch(`${apiBase}/api/error-flag/clear-all`, {
        method: 'POST'
      })
      if (response.ok) {
        setMessage('Cleared all injection history')
        fetchInjectionHistory()
      }
    } catch (error) {
      setMessage(`Failed to clear history: ${error.message}`)
    }
  }

  const recentInjections = injectionHistory.filter(h => h.device_id === selectedDevice)

  return (
    <div className="space-y-6">
      {/* Header */}
      <div className="bg-gradient-to-r from-red-500 to-orange-500 p-4 rounded-lg text-white">
        <h2 className="text-2xl font-bold mb-2">Error Injection Testing</h2>
        <p className="text-sm opacity-90">
          Cloud server calls InverterSim API to send error frames. Device naturally detects and logs errors via fault_manager.cpp
        </p>
      </div>

      {/* Recent Injections for Selected Device */}
      {recentInjections.length > 0 && (
        <div className="bg-blue-50 border-l-4 border-blue-400 p-4">
          <div className="flex justify-between items-center mb-2">
            <h3 className="font-semibold text-blue-800">Recent Injections for {selectedDevice}</h3>
            <button
              onClick={() => clearHistoryForDevice(selectedDevice)}
              className="text-xs bg-blue-200 hover:bg-blue-300 text-blue-800 px-3 py-1 rounded"
            >
              Clear History
            </button>
          </div>
          <div className="space-y-2">
            {recentInjections.slice(-5).reverse().map((inj, idx) => (
              <div key={idx} className="flex justify-between items-center bg-white p-2 rounded text-sm">
                <div>
                  <span className="font-semibold">{inj.errorType}</span>
                  {inj.errorType === 'EXCEPTION' && (
                    <span className="ml-1 text-gray-500">(code {inj.exceptionCode})</span>
                  )}
                  {inj.errorType === 'DELAY' && (
                    <span className="ml-1 text-gray-500">({inj.delayMs}ms)</span>
                  )}
                  <span className="ml-2 text-xs text-gray-400">
                    {new Date(inj.timestamp).toLocaleTimeString()}
                  </span>
                </div>
                <span className={`text-xs px-2 py-1 rounded ${
                  inj.inverter_sim_status === 200 
                    ? 'bg-green-100 text-green-700' 
                    : 'bg-red-100 text-red-700'
                }`}>
                  {inj.inverter_sim_status === 200 ? '✓ Sent' : '✗ Failed'}
                </span>
              </div>
            ))}
          </div>
        </div>
      )}

      {/* All Injections History */}
      {injectionHistory.length > 0 && (
        <div className="bg-gray-50 p-4 rounded">
          <div className="flex justify-between items-center mb-2">
            <h3 className="font-semibold text-gray-700">All Injections ({injectionHistory.length})</h3>
            <button
              onClick={clearAllHistory}
              className="text-xs bg-gray-200 hover:bg-gray-300 text-gray-700 px-3 py-1 rounded"
            >
              Clear All History
            </button>
          </div>
        </div>
      )}

      {/* Selected Device Status */}
      <div className="p-4 rounded-lg bg-purple-50 border-2 border-purple-300">
        <div className="flex items-center justify-between">
          <div>
            <span className="font-semibold">Selected Device:</span>
            <span className="ml-2 text-lg font-bold text-purple-700">{selectedDevice}</span>
          </div>
          <span className="text-purple-600 font-semibold">
            Ready for testing
          </span>
        </div>
      </div>

      {/* Error Type Selection */}
      <div className="bg-white p-6 rounded-lg shadow-md">
        <h3 className="text-lg font-semibold mb-4">Select Error Type</h3>
        
        <div className="space-y-3 mb-6">
          {errorTypes.map((type) => (
            <div
              key={type.value}
              className={`p-4 border-2 rounded-lg cursor-pointer transition-all ${
                errorType === type.value
                  ? 'border-blue-500 bg-blue-50'
                  : 'border-gray-200 hover:border-blue-300'
              }`}
              onClick={() => setErrorType(type.value)}
            >
              <div className="flex items-center">
                <input
                  type="radio"
                  name="errorType"
                  value={type.value}
                  checked={errorType === type.value}
                  onChange={(e) => setErrorType(e.target.value)}
                  className="mr-3"
                />
                <div>
                  <div className="font-semibold">{type.label}</div>
                  <div className="text-sm text-gray-600">{type.description}</div>
                </div>
              </div>
            </div>
          ))}
        </div>

        {/* Exception Code Selection (only for EXCEPTION type) */}
        {errorType === 'EXCEPTION' && (
          <div className="mb-6 p-4 bg-gray-50 rounded-lg">
            <label className="block text-sm font-semibold mb-2">Exception Code</label>
            <select
              value={exceptionCode}
              onChange={(e) => setExceptionCode(e.target.value)}
              className="w-full p-2 border rounded focus:ring-2 focus:ring-blue-500"
            >
              {exceptionCodes.map((code) => (
                <option key={code.value} value={code.value}>
                  {code.value} - {code.label}
                </option>
              ))}
            </select>
          </div>
        )}

        {/* Delay Configuration (only for DELAY type) */}
        {errorType === 'DELAY' && (
          <div className="mb-6 p-4 bg-gray-50 rounded-lg">
            <label className="block text-sm font-semibold mb-2">Delay (milliseconds)</label>
            <input
              type="number"
              value={delayMs}
              onChange={(e) => setDelayMs(e.target.value)}
              className="w-full p-2 border rounded focus:ring-2 focus:ring-blue-500"
              min="0"
              max="60000"
              step="1000"
            />
            <div className="text-sm text-gray-600 mt-1">
              Current: {(delayMs / 1000).toFixed(1)} seconds
            </div>
          </div>
        )}

        {/* Inject Button */}
        <button
          onClick={injectError}
          disabled={loading}
          className={`w-full py-3 rounded-lg font-semibold text-white transition-all ${
            loading
              ? 'bg-gray-400 cursor-not-allowed'
              : 'bg-red-600 hover:bg-red-700 active:scale-95'
          }`}
        >
          {loading ? 'Calling InverterSim...' : 'Inject Error via InverterSim'}
        </button>
      </div>

      {/* Message Display */}
      {message && (
        <div className={`p-4 rounded-lg ${
          message.startsWith('OK') ? 'bg-green-100 text-green-800' : 'bg-red-100 text-red-800'
        }`}>
          {message}
        </div>
      )}

      {/* How It Works */}
      <div className="bg-blue-50 p-4 rounded-lg text-sm">
        <h4 className="font-semibold mb-2">How It Works</h4>
        <ol className="list-decimal list-inside space-y-1 text-gray-700">
          <li>Select an error type and configure parameters</li>
          <li>Cloud server calls InverterSim API: POST http://20.15.114.131:8080/api/user/error-flag/add</li>
          <li>InverterSim sends corrupted/error frame to the device over Modbus</li>
          <li>Device naturally detects the error (CRC mismatch, timeout, exception, etc.)</li>
          <li>fault_manager.cpp logs the detected error with classification</li>
          <li>Check device serial monitor or fault logs to verify error detection</li>
        </ol>
      </div>

      {/* Expected Behavior */}
      <div className="bg-gray-50 p-4 rounded-lg text-sm">
        <h4 className="font-semibold mb-2">Expected Device Behavior</h4>
        <ul className="space-y-1 text-gray-700">
          <li>• <strong>EXCEPTION:</strong> Device logs Modbus exception and handles per protocol</li>
          <li>• <strong>CRC_ERROR:</strong> Device detects CRC mismatch and may retry</li>
          <li>• <strong>CORRUPT:</strong> Device detects corrupt data and logs error</li>
          <li>• <strong>PACKET_DROP:</strong> Device experiences timeout and retries</li>
          <li>• <strong>DELAY:</strong> Device waits for response, may timeout</li>
        </ul>
      </div>
    </div>
  )
}

export default ErrorInjectionPanel
