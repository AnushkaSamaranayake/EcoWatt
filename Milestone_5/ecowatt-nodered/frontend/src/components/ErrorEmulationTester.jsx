import React, { useState } from 'react'

const ErrorEmulationTester = () => {
  const [slaveAddress, setSlaveAddress] = useState(1)
  const [functionCode, setFunctionCode] = useState(3)
  const [errorType, setErrorType] = useState('EXCEPTION')
  const [exceptionCode, setExceptionCode] = useState(1)
  const [delayMs, setDelayMs] = useState(0)
  const [response, setResponse] = useState(null)
  const [loading, setLoading] = useState(false)

  const INVERTER_SIM_URL = 'http://20.15.114.131:8080'

  const errorTypes = [
    { value: 'EXCEPTION', label: 'Exception', description: 'Returns Modbus exception response' },
    { value: 'CRC_ERROR', label: 'CRC Error', description: 'Returns frame with corrupted CRC' },
    { value: 'CORRUPT', label: 'Corrupt Frame', description: 'Returns corrupted frame data' },
    { value: 'PACKET_DROP', label: 'Packet Drop', description: 'Simulates no response' },
    { value: 'DELAY', label: 'Delay', description: 'Delays response by specified milliseconds' }
  ]

  const functionCodes = [
    { value: 1, label: '0x01 - Read Coils' },
    { value: 2, label: '0x02 - Read Discrete Inputs' },
    { value: 3, label: '0x03 - Read Holding Registers' },
    { value: 4, label: '0x04 - Read Input Registers' },
    { value: 5, label: '0x05 - Write Single Coil' },
    { value: 6, label: '0x06 - Write Single Register' },
    { value: 15, label: '0x0F - Write Multiple Coils' },
    { value: 16, label: '0x10 - Write Multiple Registers' }
  ]

  const exceptionCodes = [
    { value: 1, label: '01 - Illegal Function' },
    { value: 2, label: '02 - Illegal Data Address' },
    { value: 3, label: '03 - Illegal Data Value' },
    { value: 4, label: '04 - Slave Device Failure' },
    { value: 5, label: '05 - Acknowledge' },
    { value: 6, label: '06 - Slave Device Busy' },
    { value: 8, label: '08 - Memory Parity Error' },
    { value: 10, label: '0A - Gateway Path Unavailable' },
    { value: 11, label: '0B - Gateway Target Device Failed' }
  ]

  const testErrorEmulation = async () => {
    setLoading(true)
    setResponse(null)

    try {
      const payload = {
        slaveAddress: parseInt(slaveAddress),
        functionCode: parseInt(functionCode),
        errorType: errorType,
        exceptionCode: errorType === 'EXCEPTION' ? parseInt(exceptionCode) : 0,
        delayMs: errorType === 'DELAY' ? parseInt(delayMs) : 0
      }

      const startTime = Date.now()
      const res = await fetch(`${INVERTER_SIM_URL}/api/inverter/error`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          'Accept': '*/*'
        },
        body: JSON.stringify(payload)
      })
      const endTime = Date.now()

      const data = await res.json()

      setResponse({
        status: res.status,
        statusText: res.ok ? 'OK' : 'Error',
        frame: data.frame || 'No frame returned',
        responseTime: endTime - startTime,
        payload: payload,
        timestamp: new Date().toISOString()
      })
    } catch (error) {
      setResponse({
        status: 'ERROR',
        statusText: 'Connection Failed',
        frame: 'N/A',
        error: error.message,
        timestamp: new Date().toISOString()
      })
    } finally {
      setLoading(false)
    }
  }

  const parseFrame = (hexFrame) => {
    if (!hexFrame || hexFrame === 'No frame returned') return null
    
    try {
      const bytes = hexFrame.match(/.{1,2}/g) || []
      return {
        slaveId: bytes[0] || '??',
        functionCode: bytes[1] || '??',
        dataBytes: bytes.slice(2, -2).join(' '),
        crc: bytes.slice(-2).join(' ')
      }
    } catch {
      return null
    }
  }

  return (
    <div className="space-y-6">
      {/* Header */}
      <div className="bg-gradient-to-r from-purple-500 to-pink-500 p-4 rounded-lg text-white">
        <h2 className="text-2xl font-bold mb-2">Error Emulation Tester</h2>
        <p className="text-sm opacity-90">
          Directly test InverterSim error responses without affecting normal operations. 
          Get immediate error frames for manual testing.
        </p>
      </div>

      {/* Configuration Panel */}
      <div className="bg-white p-6 rounded-lg shadow-md">
        <h3 className="text-lg font-semibold mb-4">Configure Test Parameters</h3>

        <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
          {/* Slave Address */}
          <div>
            <label className="block text-sm font-semibold mb-2">Slave Address</label>
            <input
              type="number"
              value={slaveAddress}
              onChange={(e) => setSlaveAddress(e.target.value)}
              className="w-full p-2 border rounded focus:ring-2 focus:ring-purple-500"
              min="1"
              max="247"
            />
            <p className="text-xs text-gray-500 mt-1">Modbus slave ID (1-247)</p>
          </div>

          {/* Function Code */}
          <div>
            <label className="block text-sm font-semibold mb-2">Function Code</label>
            <select
              value={functionCode}
              onChange={(e) => setFunctionCode(e.target.value)}
              className="w-full p-2 border rounded focus:ring-2 focus:ring-purple-500"
            >
              {functionCodes.map((fc) => (
                <option key={fc.value} value={fc.value}>
                  {fc.label}
                </option>
              ))}
            </select>
          </div>
        </div>

        {/* Error Type Selection */}
        <div className="mt-6">
          <label className="block text-sm font-semibold mb-3">Error Type</label>
          <div className="space-y-2">
            {errorTypes.map((type) => (
              <div
                key={type.value}
                className={`p-3 border-2 rounded-lg cursor-pointer transition-all ${
                  errorType === type.value
                    ? 'border-purple-500 bg-purple-50'
                    : 'border-gray-200 hover:border-purple-300'
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
        </div>

        {/* Exception Code (only for EXCEPTION type) */}
        {errorType === 'EXCEPTION' && (
          <div className="mt-4 p-4 bg-red-50 rounded-lg">
            <label className="block text-sm font-semibold mb-2">Exception Code</label>
            <select
              value={exceptionCode}
              onChange={(e) => setExceptionCode(e.target.value)}
              className="w-full p-2 border rounded focus:ring-2 focus:ring-purple-500"
            >
              {exceptionCodes.map((code) => (
                <option key={code.value} value={code.value}>
                  {code.label}
                </option>
              ))}
            </select>
          </div>
        )}

        {/* Delay (only for DELAY type) */}
        {errorType === 'DELAY' && (
          <div className="mt-4 p-4 bg-blue-50 rounded-lg">
            <label className="block text-sm font-semibold mb-2">Delay (milliseconds)</label>
            <input
              type="number"
              value={delayMs}
              onChange={(e) => setDelayMs(e.target.value)}
              className="w-full p-2 border rounded focus:ring-2 focus:ring-purple-500"
              min="0"
              max="60000"
              step="100"
            />
            <div className="text-sm text-gray-600 mt-1">
              Response will be delayed by {(delayMs / 1000).toFixed(1)} seconds
            </div>
          </div>
        )}

        {/* Test Button */}
        <button
          onClick={testErrorEmulation}
          disabled={loading}
          className={`w-full mt-6 py-3 rounded-lg font-semibold text-white transition-all ${
            loading
              ? 'bg-gray-400 cursor-not-allowed'
              : 'bg-purple-600 hover:bg-purple-700 active:scale-95'
          }`}
        >
          {loading ? 'Testing...' : 'Test Error Emulation'}
        </button>
      </div>

      {/* Response Display */}
      {response && (
        <div className="bg-white p-6 rounded-lg shadow-md">
          <div className="flex justify-between items-center mb-4">
            <h3 className="text-lg font-semibold">Response</h3>
            <span className={`px-3 py-1 rounded text-sm font-semibold ${
              response.status === 200 || response.status === 'OK'
                ? 'bg-green-100 text-green-700'
                : 'bg-red-100 text-red-700'
            }`}>
              {response.status} {response.statusText}
            </span>
          </div>

          {/* Response Time */}
          {response.responseTime && (
            <div className="mb-4 text-sm text-gray-600">
              ⏱️ Response Time: <span className="font-semibold">{response.responseTime}ms</span>
            </div>
          )}

          {/* Hex Frame */}
          <div className="mb-4">
            <label className="block text-sm font-semibold mb-2">Hex Frame</label>
            <div className="p-3 bg-gray-100 rounded font-mono text-sm break-all">
              {response.frame}
            </div>
          </div>

          {/* Parsed Frame */}
          {parseFrame(response.frame) && (
            <div className="mb-4">
              <label className="block text-sm font-semibold mb-2">Parsed Frame</label>
              <div className="space-y-2 text-sm">
                <div className="flex">
                  <span className="w-32 text-gray-600">Slave ID:</span>
                  <span className="font-mono font-semibold">0x{parseFrame(response.frame).slaveId}</span>
                </div>
                <div className="flex">
                  <span className="w-32 text-gray-600">Function Code:</span>
                  <span className="font-mono font-semibold">0x{parseFrame(response.frame).functionCode}</span>
                  {parseInt(parseFrame(response.frame).functionCode, 16) & 0x80 && (
                    <span className="ml-2 text-red-600 font-semibold">Exception Response</span>
                  )}
                </div>
                <div className="flex">
                  <span className="w-32 text-gray-600">Data:</span>
                  <span className="font-mono">{parseFrame(response.frame).dataBytes || 'N/A'}</span>
                </div>
                <div className="flex">
                  <span className="w-32 text-gray-600">CRC:</span>
                  <span className="font-mono">{parseFrame(response.frame).crc}</span>
                </div>
              </div>
            </div>
          )}

          {/* Request Payload */}
          <div>
            <label className="block text-sm font-semibold mb-2">Request Payload</label>
            <pre className="p-3 bg-gray-100 rounded text-xs overflow-auto">
              {JSON.stringify(response.payload || response, null, 2)}
            </pre>
          </div>

          {/* Error Message */}
          {response.error && (
            <div className="mt-4 p-3 bg-red-50 border-l-4 border-red-400 text-red-700">
              <span className="font-semibold">Error:</span> {response.error}
            </div>
          )}

          {/* Timestamp */}
          <div className="mt-4 text-xs text-gray-500 text-right">
            {response.timestamp}
          </div>
        </div>
      )}

      {/* Information Panel */}
      <div className="bg-blue-50 p-4 rounded-lg text-sm">
        <h4 className="font-semibold mb-2">How to Use</h4>
        <ol className="list-decimal list-inside space-y-1 text-gray-700">
          <li>Configure slave address and function code</li>
          <li>Select the type of error you want to test</li>
          <li>Configure additional parameters (exception code or delay)</li>
          <li>Click "Test Error Emulation" to get immediate error frame</li>
          <li>The API returns a Modbus frame with the specified error</li>
          <li>Use this for manual testing without affecting live operations</li>
        </ol>
      </div>

      {/* API Endpoint Info */}
      <div className="bg-gray-50 p-4 rounded-lg text-sm">
        <h4 className="font-semibold mb-2">API Endpoint</h4>
        <div className="space-y-2">
          <div className="flex">
            <span className="w-20 text-gray-600">Method:</span>
            <span className="font-mono font-semibold">POST</span>
          </div>
          <div className="flex">
            <span className="w-20 text-gray-600">URL:</span>
            <span className="font-mono text-xs">{INVERTER_SIM_URL}/api/inverter/error</span>
          </div>
          <div className="flex">
            <span className="w-20 text-gray-600">Auth:</span>
            <span className="text-gray-600">None required</span>
          </div>
        </div>
      </div>

      {/* Comparison with Error Flag API */}
      <div className="bg-yellow-50 p-4 rounded-lg text-sm">
        <h4 className="font-semibold mb-2">Error Emulation vs Error Flag</h4>
        <div className="space-y-2 text-gray-700">
          <div>
            <span className="font-semibold">Error Emulation API:</span> Returns error frame immediately for manual testing. 
            Doesn't affect device operations. Use for isolated testing.
          </div>
          <div>
            <span className="font-semibold">Error Flag API:</span> Sets a flag that triggers error in the next device request. 
            Tests actual device error detection. Use for realistic end-to-end testing.
          </div>
        </div>
      </div>
    </div>
  )
}

export default ErrorEmulationTester
