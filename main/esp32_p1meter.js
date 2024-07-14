const {} = require('zigbee-herdsman-converters/lib/modernExtend');
// Add the lines below
const fz = require('zigbee-herdsman-converters/converters/fromZigbee');
const tz = require('zigbee-herdsman-converters/converters/toZigbee');
const exposes = require('zigbee-herdsman-converters/lib/exposes');
const reporting = require('zigbee-herdsman-converters/lib/reporting');
const ota = require('zigbee-herdsman-converters/lib/ota');
const utils = require('zigbee-herdsman-converters/lib/utils');
const globalStore = require('zigbee-herdsman-converters/lib/store');
const e = exposes.presets;
const ea = exposes.access;

const tzLocal = {
    light_switch: {
        key: ['switch_r1','switch_r2','switch_r3','switch_r4','switch_r5'],
        convertSet: async (entity, key, value, meta) => {
            // 0x21FD
            // 0x6
            // When message is sent, first two bits are uint16 containing attribute ID, next is uint8 with type of data (bool is 16) and last entry in array is actuall data
            const lookup = {color_flow: 0xFFF2, switch_r2: 0x21FE, switch_r3: 0x21FF, switch_r4: 0x2200, switch_r5: 0x2201};
            const cluster = lookup[key];
            const state = value === 'OFF' ? 1 : 0;
            const options = {manufacturerCode: 4406, disableDefaultResponse: false};
            //await entity.write('seMetering', {0xFFF2: {value: 0x063e, type: 25}}, options);
            try {
                const obj = {};
                obj[cluster] = { value: state, type: 16 };
                //await entity.write('genOnOff', obj );
                await entity.write('genOnOff', {0: {value: 0, type: 16}});
                await entity.write('genLevelCtrl', {0: {value: 12, type: 16}});
                
                /*const payload = {ctrlbits: 0, ontime: 4, offwaittime: 5};
                await entity.command(
                    'genOnOff',
                    'onWithTimedOff',
                    payload,
                    utils.getOptions(meta.mapped, entity),
                );*/
            } catch (e) {
                // Fails for some TuYa devices with UNSUPPORTED_ATTRIBUTE, ignore that.
                // e.g. https://github.com/Koenkk/zigbee2mqtt/issues/14857
                if (e.message.includes('UNSUPPORTED_ATTRIBUTE')) {
                } else {
                    throw e;
                }
            }
            //await entity.write('genOnOff', {0xFFF2: {value: 1, type: 16}},{manufacturerCode: null} );
            const obj2 = {};
            obj2[key] = value
            return {state: obj2};
        },
    },
}
const fzLocal = {
    windSpeed: {
        cluster: '65522', //This part is important
        type: ['readResponse', 'attributeReport'],
        convert: (model, msg, publish, options, meta) => {
            const payload = {};
            if (msg.data.hasOwnProperty('0')) 
            {
                payload.energy_low = msg.data['0'] / 1000;
            }
            if (msg.data.hasOwnProperty('1')) {
                payload.energy_high = msg.data['1'] / 1000;
            }
            if (msg.data.hasOwnProperty('2')) {
                payload.energy_tariff = msg.data['2'];
            }
            if (msg.data.hasOwnProperty('3')) {
                payload.power = msg.data['3'];
            }
            if (msg.data.hasOwnProperty('4')) {
                payload.power_l1 = msg.data['4'][2];
                payload.power_l2 = msg.data['4'][1];
                payload.power_l3 = msg.data['4'][0];
            }
            if (msg.data.hasOwnProperty('5')) {
                payload.gas = msg.data['5'] / 1000.0;;
            }
            if (msg.data.hasOwnProperty('6')) {
                payload.fail_short = msg.data['6'];
            }
            if (msg.data.hasOwnProperty('7')) {
                payload.fail_long = msg.data['7'];
            }
            return payload
        }
    }
}

const definition = {
    zigbeeModel: ['ESP32C6.P1.Meter'],
    model: 'ESP32C6.P1.Meter',  // works leftover from trying light exampes
    vendor: 'Espressif',
    description: 'P1 Energy Meter',
    fromZigbee: [fz.on_off,fzLocal.windSpeed],
    toZigbee: [tz.on_off,tz.light_onoff_brightness, tzLocal.light_switch],
    exposes: [
        e.numeric('energy_low', ea.STATE).withUnit('kWh').withDescription('Energy low tariff'),
        e.numeric('energy_high', ea.STATE).withUnit('kWh').withDescription('Energy high tariff'),
        e.numeric('energy_tariff', ea.STATE).withDescription('Current energy tariff'),
        e.numeric('power', ea.STATE).withUnit('W').withDescription('Current power consumption'),
        e.numeric('power_l1', ea.STATE).withUnit('W').withDescription('Instantaneous active power L1 (+P)'),
        e.numeric('power_l2', ea.STATE).withUnit('W').withDescription('Instantaneous active power L2 (+P)'),
        e.numeric('power_l3', ea.STATE).withUnit('W').withDescription('Instantaneous active power L3 (+P)'),
        e.numeric('gas', ea.STATE).withUnit('m3').withDescription('Gas consumption'),
        e.numeric('fail_short', ea.STATE).withDescription('Fail short'),
        e.numeric('fail_long', ea.STATE).withDescription('Fail long'),
        //e.switch(),
        //e.light_brightness().withLabel('Update interval'),            
        e.numeric('brightness', ea.STATE_SET).withValueMin(1).withValueMax(254).withDescription(
            'How often P1 meter will read & send data').withLabel('Update interval').withUnit('s'),
    ],
    // The configure method below is needed to make the device reports on/off state changes
    // when the device is controlled manually through the button on it.
    configure: async (device, coordinatorEndpoint, logger) => {
        const endpoint = device.getEndpoint(1);
        //await reporting.bind(endpoint, coordinatorEndpoint, ['genOnOff']);
        //await endpoint.write('genOnOff', {0x6: {value: 1, type: 16}} );
        await reporting.onOff(endpoint);
    },
};
module.exports = definition;