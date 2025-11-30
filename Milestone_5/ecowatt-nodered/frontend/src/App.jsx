import React, { useState, useEffect } from 'react'
import ErrorInjectionPanel from './components/ErrorInjectionPanel'
import ErrorEmulationTester from './components/ErrorEmulationTester'

const App = () => {
  const [activeTab, setActiveTab] = useState('inverter')
  const [inverterData, setInverterData] = useState([])
  const [selectedDevice, setSelectedDevice] = useState('EcoWatt001')
  const [configData, setConfigData] = useState({
    sampling_interval: 60,
    registers: []
  })
  const [commandData, setCommandData] = useState({
    
    action: 'write_register',
    target_register: '',
    value: ''
  })
  const [fotaFile, setFotaFile] = useState(null)
  const [fotaVersion, setFotaVersion] = useState('')
  const [fotaStatus, setFotaStatus] = useState('')
  const [loading, setLoading] = useState(false)

  const devices = ['EcoWatt001', 'EcoWatt002', 'EcoWatt003', 'EcoWatt004', 'EcoWatt005']

  // API Base URL - adjust according to your setup
  const API_BASE = 'http://10.188.60.251:5000'

  // Preset configurations for easy testing
  const presetConfigs = {
    'Basic Config': {
      sampling_interval: 60,
      registers: [
        { address: '0x01', type: 'input', name: 'voltage' },
        { address: '0x02', type: 'input', name: 'current' }
      ]
    },
    'Extended Config': {
      sampling_interval: 30,
      registers: [
        { address: '0x01', type: 'input', name: 'voltage' },
        { address: '0x02', type: 'input', name: 'current' },
        { address: '0x03', type: 'holding', name: 'power_limit' },
        { address: '0x04', type: 'input', name: 'temperature' }
      ]
    }
  }

  // Preset commands for easy testing
  const presetCommands = {
    'Enable Device': { action: 'write_register', target_register: 'status_flag', value: '1' },
    'Disable Device': { action: 'write_register', target_register: 'status_flag', value: '0' },
    'Set Power Limit': { action: 'write_register', target_register: 'power_limit', value: '1000' },
    'Reset Device': { action: 'reset', target_register: '', value: '' },
    'Read Status': { action: 'read_register', target_register: 'status_flag', value: '' }
  }

  // Fetch real inverter data from API
  const fetchInverterData = async () => {
    try {
      const response = await fetch(`${API_BASE}/api/inverters`)
      if (response.ok) {
        const data = await response.json()
        setInverterData(data)
      } else {
        console.error('Failed to fetch inverter data')
        // Fallback to simulated data if API fails
        const simulatedData = [
          {
            device_id: 'EcoWatt001',
            timestamp: new Date().toISOString(),
            voltage: 230.5,
            current: 15.2,
            power: 3503.6,
            status: 'Online'
          },
          {
            device_id: 'EcoWatt002',
            timestamp: new Date().toISOString(),
            voltage: 225.8,
            current: 12.8,
            power: 2890.2,
            status: 'Online'
          }
        ]
        setInverterData(simulatedData)
      }
    } catch (error) {
      console.error('Error fetching inverter data:', error)
      // Fallback to empty array if error
      setInverterData([])
    }
  }

  // Send configuration to device
  const sendConfiguration = async () => {
    setLoading(true)
    console.log('Sending configuration:', configData)
    console.log('To device:', selectedDevice)
    
    try {
      const payload = {
        config_update: configData
      }
      console.log('Full payload:', JSON.stringify(payload, null, 2))
      
      const response = await fetch(`${API_BASE}/push_config/${selectedDevice}`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify(payload)
      })
      
      const result = await response.text()
      console.log('Response:', result)
      
      if (response.ok) {
        alert(`Configuration sent successfully! Response: ${result}`)
      } else {
        alert(`Failed to send configuration. Status: ${response.status}, Response: ${result}`)
      }
    } catch (error) {
      console.error('Error sending configuration:', error)
      alert(`Error sending configuration: ${error.message}`)
    }
    setLoading(false)
  }

  // Send command to device
  const sendCommand = async () => {
    setLoading(true)
    console.log('Sending command:', commandData)
    console.log('To device:', selectedDevice)
    
    try {
      const payload = {
        command: {
          action: commandData.action,
          target_register: commandData.target_register,
          value: commandData.value
        }
      }
      console.log('Full payload:', JSON.stringify(payload, null, 2))
      
      const response = await fetch(`${API_BASE}/push_command/${selectedDevice}`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify(payload)
      })
      
      const result = await response.text()
      console.log('Response:', result)
      
      if (response.ok) {
        alert(`Command sent successfully! Response: ${result}`)
        setCommandData({ action: 'write_register', target_register: '', value: '' })
      } else {
        alert(`Failed to send command. Status: ${response.status}, Response: ${result}`)
      }
    } catch (error) {
      console.error('Error sending command:', error)
      alert(`Error sending command: ${error.message}`)
    }
    setLoading(false)
  }

  // Load preset configuration
  const loadPresetConfig = (presetName) => {
    setConfigData(presetConfigs[presetName])
  }

  // Load preset command
  const loadPresetCommand = (presetName) => {
    setCommandData(presetCommands[presetName])
  }

  // Debug functions to check server state
  const checkConfigQueue = async () => {
    try {
      const response = await fetch(`${API_BASE}/debug/config_queue`)
      const data = await response.json()
      console.log('Config Queue:', data)
      alert(`Config Queue: ${JSON.stringify(data, null, 2)}`)
    } catch (error) {
      console.error('Error checking config queue:', error)
    }
  }

  const checkCommandsQueue = async () => {
    try {
      const response = await fetch(`${API_BASE}/debug/commands_queue`)
      const data = await response.json()
      console.log('Commands Queue:', data)
      alert(`Commands Queue: ${JSON.stringify(data, null, 2)}`)
    } catch (error) {
      console.error('Error checking commands queue:', error)
    }
  }

  const checkDataStore = async () => {
    try {
      const response = await fetch(`${API_BASE}/debug/data_store`)
      const data = await response.json()
      console.log('Data Store:', data)
      alert(`Recent Data: ${JSON.stringify(data, null, 2)}`)
    } catch (error) {
      console.error('Error checking data store:', error)
    }
  }

  // Upload FOTA file
  const uploadFotaFile = async () => {
    if (!fotaFile || !fotaVersion) {
      alert('Please select a file and enter version')
      return
    }

    setLoading(true)
    setFotaStatus('Uploading...')
    
    try {
      const formData = new FormData()
      formData.append('file', fotaFile)
      formData.append('version', fotaVersion)

      const response = await fetch(`${API_BASE}/upload_firmware`, {
        method: 'POST',
        body: formData
      })
      
      if (response.ok) {
        const result = await response.json()
        setFotaStatus(`Upload successful! Version: ${result.version}, Size: ${result.size} bytes`)
        setFotaFile(null)
        setFotaVersion('')
      } else {
        setFotaStatus('Upload failed')
      }
    } catch (error) {
      console.error('Error uploading file:', error)
      setFotaStatus('Upload error')
    }
    setLoading(false)
  }

  // Add register to config
  const addRegister = () => {
    setConfigData(prev => ({
      ...prev,
      registers: [...prev.registers, { address: '', type: 'input', name: '' }]
    }))
  }

  // Update register in config
  const updateRegister = (index, field, value) => {
    setConfigData(prev => ({
      ...prev,
      registers: prev.registers.map((reg, i) => 
        i === index ? { ...reg, [field]: value } : reg
      )
    }))
  }

  // Remove register from config
  const removeRegister = (index) => {
    setConfigData(prev => ({
      ...prev,
      registers: prev.registers.filter((_, i) => i !== index)
    }))
  }

  // Inject sample data for testing
  const injectSampleData = async () => {
    setLoading(true)
    try {
      const response = await fetch(`${API_BASE}/api/test/inject_sample_data`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        }
      })
      
      if (response.ok) {
        const result = await response.json()
        alert(result.message)
        // Refresh data after injection
        fetchInverterData()
      } else {
        alert('Failed to inject sample data')
      }
    } catch (error) {
      console.error('Error injecting sample data:', error)
      alert('Error injecting sample data')
    }
    setLoading(false)
  }

  useEffect(() => {
    fetchInverterData()
    const interval = setInterval(fetchInverterData, 30000) // Refresh every 30 seconds
    return () => clearInterval(interval)
  }, [])

  return (
    <div className="min-h-screen bg-gray-100">
      {/* Header */}
      <div className="bg-blue-600 text-white p-4">
        <h1 className="text-2xl font-bold">EcoWatt Inverter Management System</h1>
        <p className="text-blue-100">Monitor, Configure, and Update Your Solar Inverters</p>
      </div>

      {/* Device Selection */}
      <div className="bg-white shadow-sm p-4">
        <div className="flex justify-between items-center">
          <div>
            <label className="block text-sm font-medium text-gray-700 mb-2">Select Device:</label>
            <select
              value={selectedDevice}
              onChange={(e) => setSelectedDevice(e.target.value)}
              className="border border-gray-300 rounded-md px-3 py-2 focus:outline-none focus:ring-2 focus:ring-blue-500"
            >
              {devices.map(device => (
                <option key={device} value={device}>{device}</option>
              ))}
            </select>
          </div>
          <div>
            <label className="block text-sm font-medium text-gray-700 mb-2">Debug Server State:</label>
            <div className="flex gap-2">
              <button
                onClick={checkConfigQueue}
                className="bg-yellow-500 hover:bg-yellow-600 text-white px-3 py-1 rounded text-sm"
              >
                Config Queue
              </button>
              <button
                onClick={checkCommandsQueue}
                className="bg-yellow-500 hover:bg-yellow-600 text-white px-3 py-1 rounded text-sm"
              >
                Commands Queue
              </button>
              <button
                onClick={checkDataStore}
                className="bg-yellow-500 hover:bg-yellow-600 text-white px-3 py-1 rounded text-sm"
              >
                Data Store
              </button>
            </div>
          </div>
        </div>
      </div>

      {/* Navigation Tabs */}
      <div className="bg-white shadow-sm">
        <nav className="flex space-x-8 px-4">
          {[
            { id: 'inverter', label: 'Inverter Data' },
            { id: 'config', label: 'Configuration' },
            { id: 'error-test', label: 'Error Testing' },
            { id: 'error-emulation', label: 'Error Emulation' },
            { id: 'fota', label: 'FOTA Upload' }
          ].map(tab => (
            <button
              key={tab.id}
              onClick={() => setActiveTab(tab.id)}
              className={`py-4 px-2 border-b-2 font-medium text-sm ${
                activeTab === tab.id
                  ? 'border-blue-500 text-blue-600'
                  : 'border-transparent text-gray-500 hover:text-gray-700'
              }`}
            >
              {tab.label}
            </button>
          ))}
        </nav>
      </div>

      {/* Content Area */}
      <div className="p-6">
        {/* Inverter Data Tab */}
        {activeTab === 'inverter' && (
          <div className="space-y-6">
            <div className="bg-white rounded-lg shadow p-6">
              <div className="flex justify-between items-center mb-4">
                <h2 className="text-xl font-semibold text-gray-900">Inverter Output Data</h2>
                <div className="flex space-x-2">
                  <button
                    onClick={injectSampleData}
                    disabled={loading}
                    className="bg-green-500 hover:bg-green-600 disabled:bg-gray-400 text-white px-4 py-2 rounded-md text-sm"
                  >
                    {loading ? 'Injecting...' : 'Add Sample Data'}
                  </button>
                  <button
                    onClick={fetchInverterData}
                    className="bg-blue-500 hover:bg-blue-600 text-white px-4 py-2 rounded-md text-sm"
                  >
                    Refresh
                  </button>
                </div>
              </div>
              
              <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-4">
                {inverterData.map((device, index) => (
                  <div key={index} className="border rounded-lg p-4 bg-gray-50">
                    <h3 className="font-medium text-lg mb-2">{device.device_id}</h3>
                    <div className="space-y-2 text-sm">
                      <div className="flex justify-between">
                        <span>Voltage:</span>
                        <span className="font-medium">{device.voltage} V</span>
                      </div>
                      <div className="flex justify-between">
                        <span>Current:</span>
                        <span className="font-medium">{device.current} A</span>
                      </div>
                      <div className="flex justify-between">
                        <span>Power:</span>
                        <span className="font-medium">{device.power} W</span>
                      </div>
                      <div className="flex justify-between">
                        <span>Status:</span>
                        <span className={`font-medium ${device.status === 'Online' ? 'text-green-600' : 'text-red-600'}`}>
                          {device.status}
                        </span>
                      </div>
                      <div className="text-xs text-gray-500 mt-2">
                        Last update: {new Date(device.timestamp).toLocaleString()}
                      </div>
                    </div>
                  </div>
                ))}
              </div>
            </div>
          </div>
        )}

        {/* Configuration Tab */}
        {activeTab === 'config' && (
          <div className="space-y-6">
            <div className="bg-white rounded-lg shadow p-6">
              <h2 className="text-xl font-semibold text-gray-900 mb-4">Remote Configuration</h2>
              
              {/* Preset Configuration Buttons */}
              <div className="mb-6">
                <label className="block text-sm font-medium text-gray-700 mb-2">Quick Presets:</label>
                <div className="flex gap-2 flex-wrap">
                  {Object.keys(presetConfigs).map(presetName => (
                    <button
                      key={presetName}
                      onClick={() => loadPresetConfig(presetName)}
                      className="bg-gray-500 hover:bg-gray-600 text-white px-3 py-1 rounded text-sm"
                    >
                      {presetName}
                    </button>
                  ))}
                </div>
              </div>
              
              {/* Sampling Interval */}
              <div className="mb-6">
                <label className="block text-sm font-medium text-gray-700 mb-2">
                  Sampling Interval (seconds)
                </label>
                <input
                  type="number"
                  value={configData.sampling_interval}
                  onChange={(e) => setConfigData(prev => ({ ...prev, sampling_interval: parseInt(e.target.value) }))}
                  className="border border-gray-300 rounded-md px-3 py-2 w-32 focus:outline-none focus:ring-2 focus:ring-blue-500"
                />
              </div>

              {/* Registers Configuration */}
              <div className="mb-6">
                <div className="flex justify-between items-center mb-4">
                  <label className="block text-sm font-medium text-gray-700">Registers</label>
                  <button
                    onClick={addRegister}
                    className="bg-green-500 hover:bg-green-600 text-white px-3 py-1 rounded text-sm"
                  >
                    Add Register
                  </button>
                </div>
                
                {configData.registers.map((register, index) => (
                  <div key={index} className="grid grid-cols-4 gap-4 mb-3 p-3 border rounded">
                    <input
                      type="text"
                      placeholder="Address"
                      value={register.address}
                      onChange={(e) => updateRegister(index, 'address', e.target.value)}
                      className="border border-gray-300 rounded px-2 py-1 focus:outline-none focus:ring-2 focus:ring-blue-500"
                    />
                    <select
                      value={register.type}
                      onChange={(e) => updateRegister(index, 'type', e.target.value)}
                      className="border border-gray-300 rounded px-2 py-1 focus:outline-none focus:ring-2 focus:ring-blue-500"
                    >
                      <option value="input">Input</option>
                      <option value="holding">Holding</option>
                    </select>
                    <input
                      type="text"
                      placeholder="Name"
                      value={register.name}
                      onChange={(e) => updateRegister(index, 'name', e.target.value)}
                      className="border border-gray-300 rounded px-2 py-1 focus:outline-none focus:ring-2 focus:ring-blue-500"
                    />
                    <button
                      onClick={() => removeRegister(index)}
                      className="bg-red-500 hover:bg-red-600 text-white px-2 py-1 rounded text-sm"
                    >
                      Remove
                    </button>
                  </div>
                ))}
              </div>

              <button
                onClick={sendConfiguration}
                disabled={loading}
                className="bg-blue-500 hover:bg-blue-600 disabled:bg-gray-400 text-white px-6 py-2 rounded-md"
              >
                {loading ? 'Sending...' : 'Send Configuration'}
              </button>
            </div>

            {/* Commands Section */}
            <div className="bg-white rounded-lg shadow p-6">
              <h2 className="text-xl font-semibold text-gray-900 mb-4">Send Command</h2>
              
              {/* Preset Command Buttons */}
              <div className="mb-6">
                <label className="block text-sm font-medium text-gray-700 mb-2">Quick Commands:</label>
                <div className="flex gap-2 flex-wrap">
                  {Object.keys(presetCommands).map(presetName => (
                    <button
                      key={presetName}
                      onClick={() => loadPresetCommand(presetName)}
                      className="bg-gray-500 hover:bg-gray-600 text-white px-3 py-1 rounded text-sm"
                    >
                      {presetName}
                    </button>
                  ))}
                </div>
              </div>
              
              <div className="grid grid-cols-1 md:grid-cols-3 gap-4 mb-4">
                <div>
                  <label className="block text-sm font-medium text-gray-700 mb-2">Action</label>
                  <select
                    value={commandData.action}
                    onChange={(e) => setCommandData(prev => ({ ...prev, action: e.target.value }))}
                    className="w-full border border-gray-300 rounded-md px-3 py-2 focus:outline-none focus:ring-2 focus:ring-blue-500"
                  >
                    <option value="write_register">Write Register</option>
                    <option value="read_register">Read Register</option>
                    <option value="reset">Reset</option>
                  </select>
                </div>
                
                <div>
                  <label className="block text-sm font-medium text-gray-700 mb-2">Target Register</label>
                  <input
                    type="text"
                    value={commandData.target_register}
                    onChange={(e) => setCommandData(prev => ({ ...prev, target_register: e.target.value }))}
                    placeholder="e.g., status_flag"
                    className="w-full border border-gray-300 rounded-md px-3 py-2 focus:outline-none focus:ring-2 focus:ring-blue-500"
                  />
                </div>
                
                <div>
                  <label className="block text-sm font-medium text-gray-700 mb-2">Value</label>
                  <input
                    type="text"
                    value={commandData.value}
                    onChange={(e) => setCommandData(prev => ({ ...prev, value: e.target.value }))}
                    placeholder="e.g., 1"
                    className="w-full border border-gray-300 rounded-md px-3 py-2 focus:outline-none focus:ring-2 focus:ring-blue-500"
                  />
                </div>
              </div>

              <button
                onClick={sendCommand}
                disabled={loading}
                className="bg-purple-500 hover:bg-purple-600 disabled:bg-gray-400 text-white px-6 py-2 rounded-md"
              >
                {loading ? 'Sending...' : 'Send Command'}
              </button>
            </div>
          </div>
        )}

        {/* Error Testing Tab */}
        {activeTab === 'error-test' && (
          <ErrorInjectionPanel apiBase={API_BASE} selectedDevice={selectedDevice} />
        )}

        {/* Error Emulation Tab */}
        {activeTab === 'error-emulation' && (
          <ErrorEmulationTester />
        )}

        {/* FOTA Upload Tab */}
        {activeTab === 'fota' && (
          <div className="bg-white rounded-lg shadow p-6">
            <h2 className="text-xl font-semibold text-gray-900 mb-4">Firmware Over-The-Air (FOTA) Upload</h2>
            
            <div className="space-y-4">
              <div>
                <label className="block text-sm font-medium text-gray-700 mb-2">
                  Firmware Version
                </label>
                <input
                  type="text"
                  value={fotaVersion}
                  onChange={(e) => setFotaVersion(e.target.value)}
                  placeholder="e.g., 1.2.3"
                  className="border border-gray-300 rounded-md px-3 py-2 w-64 focus:outline-none focus:ring-2 focus:ring-blue-500"
                />
              </div>

              <div>
                <label className="block text-sm font-medium text-gray-700 mb-2">
                  Firmware File (.bin)
                </label>
                <input
                  type="file"
                  accept=".bin"
                  onChange={(e) => setFotaFile(e.target.files[0])}
                  className="border border-gray-300 rounded-md px-3 py-2 w-full focus:outline-none focus:ring-2 focus:ring-blue-500"
                />
              </div>

              <button
                onClick={uploadFotaFile}
                disabled={loading || !fotaFile || !fotaVersion}
                className="bg-orange-500 hover:bg-orange-600 disabled:bg-gray-400 text-white px-6 py-2 rounded-md"
              >
                {loading ? 'Uploading...' : 'Upload Firmware'}
              </button>

              {fotaStatus && (
                <div className={`mt-4 p-3 rounded ${fotaStatus.includes('successful') ? 'bg-green-100 text-green-800' : 'bg-red-100 text-red-800'}`}>
                  {fotaStatus}
                </div>
              )}
            </div>

            <div className="mt-6 p-4 bg-gray-50 rounded-lg">
              <h3 className="font-medium text-gray-900 mb-2">FOTA Process:</h3>
              <ol className="list-decimal list-inside text-sm text-gray-600 space-y-1">
                <li>Upload firmware file with version number</li>
                <li>File is processed and chunked for secure transmission</li>
                <li>Devices will automatically check for new firmware</li>
                <li>Download and installation happens over-the-air</li>
                <li>Device reports back installation status</li>
              </ol>
            </div>
          </div>
        )}
      </div>
    </div>
  )
}

export default App