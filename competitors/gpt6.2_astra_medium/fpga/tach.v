module channel(input clk, tick, tach, input [9:0] ratio,
               output reg drive, output reg [15:0] period, generated);
    initial begin
        drive=0;
        period=0;
        generated=0;
    end
    // Deux bascules de synchronisation puis qualification sur trois ticks.
    reg [1:0] sync=3;
    reg [2:0] samples=7;
    reg level=1, armed=0, seen_out=0;
    reg [15:0] age=0, out_age=0;
    reg [22:0] phase=0, limit=0;
    wire [22:0] next_phase=phase+{ratio,1'b0};
    wire [22:0] target=period*7'd100;
    always @(posedge clk) begin
        sync <= {sync[0],tach};
        if (tick) begin
            samples <= {samples[1:0],sync[1]};
            if (&samples) level <= 1;
            if (~|samples) level <= 0;
            if (age < 62500) age <= age+1'b1;
            if (level && (~|samples) && (!armed || age>=249)) begin
                if (armed && age<62499) period <= age+1'b1;
                else period <= 0;
                armed <= 1;
                age <= 0;
            end else if (age>=62499) begin
                armed <= 0;
                period <= 0;
            end
            if (period==0 || ratio==0) begin
                drive <= 0;
                phase <= 0;
                limit <= 0;
                out_age <= 0;
                seen_out <= 0;
                generated <= 0;
            end else begin
                if (out_age!=65535) out_age <= out_age+1'b1;
                if (limit==0) limit <= target;
                else if (next_phase>=limit) begin
                    // Conserver le reste fractionnaire pour éviter une dérive.
                    phase <= next_phase-limit;
                    limit <= target;
                    drive <= !drive;
                    if (!drive) begin
                        if (seen_out) generated <= out_age+1'b1;
                        seen_out <= 1;
                        out_age <= 0;
                    end
                end else phase <= next_phase;
            end
        end
    end
endmodule

module top(input clk, tach9,tach6, cs,sck,mosi, output miso,q9,q6,led_n);
    reg [6:0] divider=0;
    wire tick=(divider==95); // 12 MHz / 96 : résolution de 8 µs
    always @(posedge clk) divider <= tick ? 0 : divider+1'b1;
    reg [9:0] r9=0,r6=0;
    wire [15:0] p9,p6,g9,g6;
    channel a(clk,tick,tach9,r9,q9,p9,g9);
    channel b(clk,tick,tach6,r6,q6,p6,g6);
    assign led_n = !((p9!=0 && r9!=0) || (p6!=0 && r6!=0));

    // SPI mode 0, MSB en premier. Instantané cohérent avant sélection.
    reg [2:0] select_sync=7, clock_sync=0;
    reg [1:0] data_sync=0;
    reg [71:0] tx=0,rx=0;
    reg [6:0] bits=0;
    wire [15:0] a_cfg=rx[63:48], b_cfg=rx[47:32];
    wire [7:0] check=rx[71:64]^rx[63:56]^rx[55:48]^rx[47:40]^rx[39:32];
    assign miso=tx[71]; // Liaison point à point
    always @(posedge clk) begin
        select_sync <= {select_sync[1:0],cs};
        clock_sync <= {clock_sync[1:0],sck};
        data_sync <= {data_sync[0],mosi};
        if (select_sync[1]) begin
            if (!select_sync[2] && bits==72 && rx[71:64]==8'hA5 &&
                rx[31:24]==check && rx[23:0]==0 &&
                a_cfg>=100 && a_cfg<=800 && b_cfg>=100 && b_cfg<=800) begin
                r9 <= a_cfg[9:0];
                r6 <= b_cfg[9:0];
            end
            bits <= 0;
            tx <= {8'hD3,p9,g9,p6,g6};
        end else begin
            if (clock_sync[2:1]==2'b01) begin
                if (bits!=127) bits <= bits+1'b1;
                rx <= {rx[70:0],data_sync[1]};
            end
            if (clock_sync[2:1]==2'b10) tx <= {tx[70:0],1'b0};
        end
    end
endmodule
