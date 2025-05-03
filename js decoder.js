function Decoder(bytes, port) {
    var decoded = {};
    var rawData = '';

    // Convert raw bytes to a readable hex string
    for (var i = 0; i < bytes.length; i++) {
        var raw = parseInt(bytes[i]);
        rawData += raw.toString(16).padStart(2, '0') + ' ';
    }
    decoded.raw_payload = rawData.trim();
    var data = bytes;

    const toInt16LE = (lower, upper) => {
        let value = (upper << 8) | (lower & 0xFF);
        if (value & 0x8000) value -= 0x10000;
        return value;
    };

    if (port === 2) {
        decoded.port = port;
        if (data[0] === 0xA1) {
            decoded.PAYLOAD_TYPE = "ALARM THEFT"; 
            data = data.slice(1);
            decoded.DC1= data[0] & 0x01 ? "ON" : "OFF";
            decoded.DC2= data[0] & 0x02 ? "ON" : "OFF";
            decoded.DC3= data[0] & 0x04 ? "ON" : "OFF";
            decoded.DC4= data[0] & 0x08 ? "ON" : "OFF";
            decoded.DC5= data[0] & 0x10 ? "ON" : "OFF";
            decoded.DC6= data[0] & 0x20 ? "ON" : "OFF";
           
        }
        else if (data[0] === 0xA2){
            decoded.PAYLOAD_TYPE = "SILENT ALARM"; 
            data = data.slice(1);
            decoded.DC1= data[0] & 0x01 ? "ON" : "OFF";
            decoded.DC2= data[0] & 0x02 ? "ON" : "OFF";
            decoded.DC3= data[0] & 0x04 ? "ON" : "OFF";
            decoded.DC4= data[0] & 0x08 ? "ON" : "OFF";
            decoded.DC5= data[0] & 0x10 ? "ON" : "OFF";
            decoded.DC6= data[0] & 0x20 ? "ON" : "OFF";
        } 
        else if (data[0] === 0xA3){
            decoded.PAYLOAD_TYPE = "HEART BEAT"; 
            data = data.slice(1);
            decoded.TEMPERATURE= toInt16LE(data[0], data[1]) / 10; 
            decoded.HUMIDITY= toInt16LE(data[2], data[3]) / 10 ; 
            decoded.SIREN= data[4]; 
            decoded.DC1= data[5] & 0x01 ? "ON" : "OFF";
            decoded.DC2= data[5] & 0x02 ? "ON" : "OFF";
            decoded.DC3= data[5] & 0x04 ? "ON" : "OFF";
            decoded.DC4= data[5] & 0x08 ? "ON" : "OFF";
            decoded.DC5= data[5] & 0x10 ? "ON" : "OFF";
            decoded.DC6= data[5] & 0x20 ? "ON" : "OFF";

        } 
        else if (data[0] === 0xA4){
            decoded.PAYLOAD_TYPE = "DIAGNOSTIC"; 
            data = data.slice(1);
            decoded.DC_VOLT= toInt16LE(data[0], data[1]) /10; 
            decoded.DC_CURRENT= toInt16LE(data[2], data[3])/10; 
            decoded.BATT_VOLT= toInt16LE(data[4], data[5])/10; 
            decoded.BATT_CURR= toInt16LE(data[6], data[7])/10;

        } 
        else if (data[0] === 0xA5){ // change ng dry contact. tapos copy paste heart beat 
            decoded.PAYLOAD_TYPE = "EVENTS"; 
            data = data.slice(1);
            if(data[0] == 0x00){
                decoded.EVENT_TYPE = "NO EVENT";
            }
            else if(data[0] == 0x01){
                decoded.EVENT_TYPE = "DC CHANGE OF STATE";
            }
            else if(data[0] == 0x02){
                decoded.EVENT_TYPE = "TEMP_THRESHOLD";
            }
            else if(data[0] == 0x03){
                decoded.EVENT_TYPE = "HUM_THRESHOLD";
            }
            data = data.slice(1);
            decoded.TEMPERATURE= toInt16LE(data[0], data[1])/10; 
            decoded.HUMIDITY= toInt16LE(data[2], data[3])/10; 

            decoded.DC1= data[4] & 0x01 ? "ON" : "OFF";
            decoded.DC2= data[4] & 0x02 ? "ON" : "OFF";
            decoded.DC3= data[4] & 0x04 ? "ON" : "OFF";
            decoded.DC4= data[4] & 0x08 ? "ON" : "OFF";
            decoded.DC5= data[4] & 0x10 ? "ON" : "OFF";
            decoded.DC6= data[4] & 0x20 ? "ON" : "OFF";
        } 
    }

    if (port === 69) { 
        decoded.TRANSMISSION_TYPE = "SOS DATA";

        decoded.SOS_TYPE = data[0] === 0 ? "SENSOR" : "DIAGNOSTIC";
        data = data.slice(1);
        decoded.DATA_RATE = data[0];
        data = data.slice(1);
        
    }

return decoded;
}

